#pragma once
#include "../arch/type.h"
#include "../mem/type.h"

/*----------------------关于磁盘----------------------*/

#define VIRTIO_BASE 0x10001000
#define VIRTIO_IRQ 1

#define R(r) ((volatile uint32 *)(VIRTIO_BASE + (r)))

#define VRING_DESC_F_NEXT 1
#define VRING_DESC_F_WRITE 2

#define VIRTIO_BLK_T_IN 0
#define VIRTIO_BLK_T_OUT 1

#define VIRTIO_NUM 8

typedef struct vring_desc {
    uint64 addr;
    uint32 len;
    uint16 flags;
    uint16 next;
} vring_desc_t;

typedef struct vring_used_elem {
    uint32 id;
    uint32 len;
} vring_used_elem_t;

typedef struct used_area {
    uint16 flags;
    uint16 id;
    vring_used_elem_t elems[VIRTIO_NUM];
} used_area_t;

typedef struct disk {
    // 驱动需要8KB的连续空间, 不适合用pmem_alloc来申请
    // 所以直接定义在这里
    char pages[2 * PGSIZE];
    
    vring_desc_t *desc;
    used_area_t *used;
    uint16 *avail;
    char free[VIRTIO_NUM];
    uint16 used_idx;
    struct
    {
        struct buffer *b;
        char status;
    } info[VIRTIO_NUM];
    spinlock_t vdisk_lock;  
} disk_t;

#define VIRTIO_MMIO_MAGIC_VALUE 0x000
#define VIRTIO_MMIO_VERSION 0x004
#define VIRTIO_MMIO_DEVICE_ID 0x008
#define VIRTIO_MMIO_VENDOR_ID 0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_GUEST_PAGE_SIZE 0x028
#define VIRTIO_MMIO_QUEUE_SEL 0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034
#define VIRTIO_MMIO_QUEUE_NUM 0x038
#define VIRTIO_MMIO_QUEUE_ALIGN 0x03c
#define VIRTIO_MMIO_QUEUE_PFN 0x040
#define VIRTIO_MMIO_QUEUE_READY 0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY 0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060
#define VIRTIO_MMIO_INTERRUPT_ACK 0x064
#define VIRTIO_MMIO_STATUS 0x070

#define VIRTIO_CONFIG_S_ACKNOWLEDGE 1
#define VIRTIO_CONFIG_S_DRIVER 2
#define VIRTIO_CONFIG_S_DRIVER_OK 4
#define VIRTIO_CONFIG_S_FEATURES_OK 8

#define VIRTIO_BLK_F_RO 5
#define VIRTIO_BLK_F_SCSI 7  
#define VIRTIO_BLK_F_CONFIG_WCE 11
#define VIRTIO_BLK_F_MQ 12
#define VIRTIO_F_ANY_LAYOUT 27
#define VIRTIO_RING_F_INDIRECT_DESC 28
#define VIRTIO_RING_F_EVENT_IDX 29

/*-------------------关于块缓冲区--------------------*/

#define BLOCK_SIZE 4096              // 基本管理单位的大小
#define N_BUFFER_TEST 8              // 测试时的N_BUFFER取值
#define N_BUFFER (32 * 512)          // 最多可以用32MB内存空间(25%)作为Block缓冲区
#define BLOCK_NUM_UNUSED 0xFFFFFFFF  // 未使用的Buffer需要将block_num设为这个值

/* 以Block为单位在内存和磁盘间传递数据 */
typedef struct buffer {
    /*
        锁的说明:
        1. block_num和ref由全局的自旋锁lk_buf_cache保护
        2. data和disk由内部的睡眠锁slk保护
    */
    uint32 block_num;                // buffer对应的磁盘内block序号 
    uint32 ref;                      // 引用数 (该buffer被get的次数)
    sleeplock_t slk;                 // 睡眠锁
    uint8* data;                     // block数据(大小为BLOCK_SIZE)
    bool disk;                       // 在virtio.c中使用
} buffer_t;

/* 将buffer这种数据结构包装成资源节点 */
typedef struct buffer_node {
    buffer_t buf;                     // 资源
    struct buffer_node *next;         // 链接
    struct buffer_node *prev;         // 链接
} buffer_node_t;

/*-------------------关于文件系统--------------------*/

#define FS_MAGIC 0x12341234                 // 魔数
#define FS_SB_BLOCK 0                       // 超级块的序号

/* 超级块 */
typedef struct super_block {
    unsigned int magic_num;                  // 用于标识文件系统类型
    unsigned int block_size;                 // 基本存储单位的大小 (字节)
    unsigned int total_blocks;               // 总共的块数量
    unsigned int total_inodes;               // 总共的inode数量

    unsigned int inode_bitmap_firstblock;    // inode_bitmap区域的起始块号
	unsigned int inode_bitmap_blocks;        // inode_bitmap区域的块数量
	unsigned int inode_firstblock;           // inode区域的起始块号
    unsigned int inode_blocks;               // inode区域的块数量
	unsigned int data_bitmap_firstblock;     // data_bitmap区域的起始块号
	unsigned int data_bitmap_blocks;         // data_bitmap区域的块数量
	unsigned int data_firstblock;            // data区域的起始块号
    unsigned int data_blocks;                // data区域的块数量
} super_block_t;

/* type的可能取值 */
#define INODE_TYPE_DATA      0               // inode管理无结构的流式数据
#define INODE_TYPE_DIR       1               // inode管理结构化的目录数据
#define INODE_TYPE_DIVICE    2               // inode对应虚拟设备(不管理数据)

/* major和minor的默认取值(代表磁盘设备) */
#define INODE_MAJOR_DEFAULT   1              // 默认的主设备号
#define INODE_MINOR_DEFAULT   1              // 默认的次设备号

/* index字段相关 */
#define INODE_INDEX_1        (10)                 // 直接映射 (10个格子)
#define INODE_INDEX_2        (10+2)               // 一级间接映射 (2个格子)
#define INODE_INDEX_3        (10+2+1)             // 二级间接映射 (1个格子)
#define INODE_BLOCK_INDEX_1  (10)                 // 直接映射 (40KB)
#define INODE_BLOCK_INDEX_2  (10+2048)            // 一级间接映射 (40KB + 8MB)
#define INODE_BLOCK_INDEX_3  (10+2048+1024*1024)  // 二级间接映射 (40KB + 8MB + 4GB)

/* 最大的size */
#define INODE_MAX_SIZE (0xFFFFFFFF)

/* 磁盘上的索引节点(64 Byte) */
typedef struct inode_disk {
    short type;                          // 文件类型
    short major;                         // 主设备号
    short minor;                         // 次设备号
    short nlink;                         // 链接数
    unsigned int size;                   // 文件数据长度(字节)
    unsigned int index[INODE_INDEX_3];   // 数据存储位置(10+2+1)
} inode_disk_t;

#define ROOT_INODE  0                 // 根节点的序号
#define N_INODE     64                // 内存中最多存在的inode数量

/* 内存里的索引节点 */
typedef struct inode {
    inode_disk_t disk_info;           // 持久化信息 (slk保护)
    bool valid_info;                  // disk_info的有效性 (slk保护)
    uint32 inode_num;                 // inode序号 (slk保护)
    uint32 ref;                       // 引用数 (lk_inode_cache保护)
    sleeplock_t slk;                  // 睡眠锁
} inode_t;

#define MAXLEN_FILENAME 60            // 文件名的最大长度
#define INVALID_INODE_NUM 0xFFFFFFFF  // 无效inode_num

/* 目录项(64 Byte) */
typedef struct dentry {
    char name[MAXLEN_FILENAME];       // 文件名
    unsigned int inode_num;           // 索引节点序号
} dentry_t;


/* 辅助计算 */
#define BIT_PER_BYTE 8
#define BIT_PER_BLOCK (BLOCK_SIZE * BIT_PER_BYTE)
#define INODE_PER_BLOCK (BLOCK_SIZE / sizeof(inode_disk_t))
#define DENTRY_PER_BLOCK  (BLOCK_SIZE / sizeof(dentry_t))
#define COUNT_BLOCKS(ele_num, ele_per_block)  (((ele_num) + (ele_per_block) - 1) / (ele_per_block)) 
