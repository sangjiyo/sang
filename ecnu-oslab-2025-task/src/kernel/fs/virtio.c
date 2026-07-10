#include "mod.h"

static __attribute__((aligned(PGSIZE))) disk_t disk;

/* 初始化虚拟磁盘 */
void virtio_disk_init()
{
    uint32 status = 0;

    spinlock_init(&disk.vdisk_lock, "virtio_disk");

    if (*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
        *R(VIRTIO_MMIO_VERSION) != 1 ||
        *R(VIRTIO_MMIO_DEVICE_ID) != 2 ||
        *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551) {
        panic("could not find virtio disk");
    }

    status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
    *R(VIRTIO_MMIO_STATUS) = status;

    status |= VIRTIO_CONFIG_S_DRIVER;
    *R(VIRTIO_MMIO_STATUS) = status;

    // negotiate features
    uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
    features &= ~(1 << VIRTIO_BLK_F_RO);
    features &= ~(1 << VIRTIO_BLK_F_SCSI);
    features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
    features &= ~(1 << VIRTIO_BLK_F_MQ);
    features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
    features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
    features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
    *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

    // tell device that feature negotiation is complete.
    status |= VIRTIO_CONFIG_S_FEATURES_OK;
    *R(VIRTIO_MMIO_STATUS) = status;

    // tell device we're completely ready.
    status |= VIRTIO_CONFIG_S_DRIVER_OK;
    *R(VIRTIO_MMIO_STATUS) = status;

    *R(VIRTIO_MMIO_GUEST_PAGE_SIZE) = PGSIZE;

    // initialize queue 0.
    *R(VIRTIO_MMIO_QUEUE_SEL) = 0;
    uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max == 0)
        panic("virtio disk has no queue 0");
    if (max < VIRTIO_NUM)
        panic("virtio disk max queue too short");
    *R(VIRTIO_MMIO_QUEUE_NUM) = VIRTIO_NUM;
    memset(disk.pages, 0, sizeof(disk.pages));
    *R(VIRTIO_MMIO_QUEUE_PFN) = ((uint64)disk.pages) >> 12;

    // desc = pages -- num * VRingDesc
    // avail = pages + 0x40 -- 2 * uint16, then num * uint16
    // used = pages + 4096 -- 2 * uint16, then num * vRingUsedElem

    // disk.pages (共 2 页，连续 8192 字节)
    // │
    // ├─ Page 0 (0 ~ 4095)
    // │   ├─ desc[NUM]   (NUM 个 VRingDesc，描述 I/O 缓冲区)
    // │   └─ avail ring  (驱动提交给设备的请求队列)
    // │
    // └─ Page 1 (4096 ~ 8191)
    //     └─ used ring   (设备完成请求后填入的队列)

    disk.desc = (vring_desc_t*)disk.pages;
    disk.avail = (uint16*)(((char *)disk.desc) + VIRTIO_NUM * sizeof(vring_desc_t));
    disk.used = (used_area_t*)(disk.pages + PGSIZE);

    for (int i = 0; i < VIRTIO_NUM; i++)
        disk.free[i] = 1;
}

static int alloc_desc()
{
    for (int i = 0; i < VIRTIO_NUM; i++)
    {
        if (disk.free[i])
        {
            disk.free[i] = 0;
            return i;
        }
    }
    return -1;
}

static void free_desc(int i)
{
    if (i >= VIRTIO_NUM)
        panic("virtio_disk_intr 1");
    if (disk.free[i])
        panic("virtio_disk_intr 2");
    disk.desc[i].addr = 0;
    disk.free[i] = 1;
    proc_wakeup(&disk.free[0]);
}

static void free_chain(int i)
{
    while (1)
    {
        free_desc(i);
        if (disk.desc[i].flags & VRING_DESC_F_NEXT)
            i = disk.desc[i].next;
        else
            break;
    }
}

static int alloc3_desc(int *idx)
{
    for (int i = 0; i < 3; i++)
    {
        idx[i] = alloc_desc();
        if (idx[i] < 0) {
            for (int j = 0; j < i; j++)
                free_desc(idx[j]);
            return -1;
        }
    }
    return 0;
}

/* 基于buffer的block读写操作 */
void virtio_disk_rw(buffer_t *b, bool write)
{
    uint64 sector = b->block_num * (BLOCK_SIZE / 512);

    spinlock_acquire(&disk.vdisk_lock);

    // the spec says that legacy block operations use three
    // descriptors: one for type/reserved/sector, one for
    // the data, one for a 1-byte status result.

    // allocate the three descriptors.
    int idx[3];
    while (1)
    {
        if (alloc3_desc(idx) == 0)
            break;

        proc_sleep(&disk.free[0], &disk.vdisk_lock);
    }

    // format the three descriptors.
    // qemu's virtio-blk.c reads them.

    struct virtio_blk_outhdr {
        uint32 type;
        uint32 reserved;
        uint64 sector;
    } buf0;

    if (write)
        buf0.type = VIRTIO_BLK_T_OUT; // write the disk
    else
        buf0.type = VIRTIO_BLK_T_IN; // read the disk
    buf0.reserved = 0;
    buf0.sector = sector;

    // buf0 is on a kernel stack, which is not direct mapped,
    // thus the call to kvmpa().
    uint64 addr = ALIGN_DOWN((uint64)&buf0, PGSIZE);
    uint64 off = ((uint64)&buf0) % PGSIZE;
    pte_t *pte = vm_getpte(NULL, addr, false);

    disk.desc[idx[0]].addr = (uint64)PTE_TO_PA(*pte) + off;
    disk.desc[idx[0]].len = sizeof(buf0);
    disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
    disk.desc[idx[0]].next = idx[1];

    disk.desc[idx[1]].addr = (uint64)b->data;
    disk.desc[idx[1]].len = BLOCK_SIZE;
    if (write)
        disk.desc[idx[1]].flags = 0; // device reads b->data
    else
        disk.desc[idx[1]].flags = VRING_DESC_F_WRITE; // device writes b->data
    disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
    disk.desc[idx[1]].next = idx[2];

    disk.info[idx[0]].status = 0;
    disk.desc[idx[2]].addr = (uint64)&disk.info[idx[0]].status;
    disk.desc[idx[2]].len = 1;
    disk.desc[idx[2]].flags = VRING_DESC_F_WRITE; // device writes the status
    disk.desc[idx[2]].next = 0;

    // record for virtio_disk_intr().
    b->disk = true;
    disk.info[idx[0]].b = b;

    // avail[0] is flags
    // avail[1] tells the device how far to look in avail[2...].
    // avail[2...] are desc[] indices the device should process.
    // we only tell device the first index in our chain of descriptors.
    disk.avail[2 + (disk.avail[1] % VIRTIO_NUM)] = idx[0];
    __sync_synchronize();
    disk.avail[1] = disk.avail[1] + 1;

    *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // value is queue number

    // Wait for virtio_disk_intr() to say request has finished.
    while (b->disk == true)
        proc_sleep(b, &disk.vdisk_lock);

    disk.info[idx[0]].b = 0;
    free_chain(idx[0]);

    spinlock_release(&disk.vdisk_lock);
}

/* 磁盘中断处理 */
void virtio_disk_intr()
{
    spinlock_acquire(&disk.vdisk_lock);

    while ((disk.used_idx % VIRTIO_NUM) != (disk.used->id % VIRTIO_NUM))
    {
        int id = disk.used->elems[disk.used_idx].id;

        if (disk.info[id].status != 0)
            panic("virtio_disk_intr status");

        disk.info[id].b->disk = false; // disk is done with buf
        proc_wakeup(disk.info[id].b);

        disk.used_idx = (disk.used_idx + 1) % VIRTIO_NUM;
    }
    *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

    spinlock_release(&disk.vdisk_lock);
}