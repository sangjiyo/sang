#include "mod.h"

super_block_t sb;
file_t file_table[N_FILE];
spinlock_t lk_file_table;

void file_init()
{
    spinlock_init(&lk_file_table, "lk_file_table");
    for (int i = 0; i < N_FILE; i++) {
        memset(&file_table[i], 0, sizeof(file_t));
    }
}

file_t* file_alloc()
{
    spinlock_acquire(&lk_file_table);
    for (int i = 0; i < N_FILE; i++) {
        if (file_table[i].ref == 0) {
            file_table[i].ref = 1;
            spinlock_release(&lk_file_table);
            return &file_table[i];
        }
    }
    spinlock_release(&lk_file_table);
    return NULL;
}

file_t* file_open(char* path, uint32 open_mode)
{
    inode_t* ip;

    ip = path_to_inode(path);
    if (ip == NULL) {
        if (!(open_mode & FILE_OPEN_CREATE))
            return NULL;
        ip = path_create_inode(path, INODE_TYPE_DATA,
            INODE_MAJOR_DEFAULT, INODE_MINOR_DEFAULT);
        if (ip == NULL)
            return NULL;
    }

    inode_lock(ip);

    if (ip->disk_info.type == INODE_TYPE_DIVICE) {
        if (!device_open_check(ip->disk_info.major, open_mode)) {
            inode_unlock(ip);
            inode_put(ip);
            return NULL;
        }
    }

    file_t* file = file_alloc();
    if (file == NULL) {
        inode_unlock(ip);
        inode_put(ip);
        return NULL;
    }

    file->ip = ip;
    file->readable = (open_mode & FILE_OPEN_READ) != 0;
    file->writbale = (open_mode & FILE_OPEN_WRITE) != 0;
    file->offset = 0;
    inode_unlock(ip);
    return file;
}

void file_close(file_t* file)
{
    if (file->ip == NULL) return;

    spinlock_acquire(&lk_file_table);
    file->ref--;
    if (file->ref == 0) {
        inode_t* ip = file->ip;
        file->ip = NULL;
        file->readable = false;
        file->writbale = false;
        file->offset = 0;
        spinlock_release(&lk_file_table);
        inode_put(ip);
        return;
    }
    spinlock_release(&lk_file_table);
}

uint32 file_read(file_t* file, uint32 len, uint64 dst, bool is_user_dst)
{
    if (!file->readable) return 0;

    inode_t* ip = file->ip;
    inode_lock(ip);

    uint32 ret = 0;
    switch (ip->disk_info.type) {
    case INODE_TYPE_DATA:
        ret = inode_read_data(ip, file->offset, len, (void*)dst, is_user_dst);
        break;
    case INODE_TYPE_DIR:
        ret = dentry_transmit(ip, dst, len, is_user_dst);
        break;
    case INODE_TYPE_DIVICE:
        ret = device_read_data(ip->disk_info.major, len, dst, is_user_dst);
        break;
    default:
        panic("file_read: unknown type");
    }

    if (ret > 0 && ip->disk_info.type == INODE_TYPE_DATA)
        file->offset += ret;

    inode_unlock(ip);
    return ret;
}

uint32 file_write(file_t* file, uint32 len, uint64 src, bool is_user_src)
{
    if (!file->writbale) return 0;

    inode_t* ip = file->ip;
    inode_lock(ip);

    uint32 ret = 0;
    switch (ip->disk_info.type) {
    case INODE_TYPE_DATA:
        ret = inode_write_data(ip, file->offset, len, (void*)src, is_user_src);
        break;
    case INODE_TYPE_DIR:
        ret = 0;
        break;
    case INODE_TYPE_DIVICE:
        ret = device_write_data(ip->disk_info.major, len, src, is_user_src);
        break;
    default:
        panic("file_write: unknown type");
    }

    if (ret > 0 && ip->disk_info.type == INODE_TYPE_DATA)
        file->offset += ret;

    inode_unlock(ip);
    return ret;
}

uint32 file_lseek(file_t* file, uint32 lseek_offset, uint32 lseek_flag)
{
    inode_t* ip = file->ip;

    switch (lseek_flag) {
    case FILE_LSEEK_SET: file->offset = lseek_offset; break;
    case FILE_LSEEK_ADD: file->offset += lseek_offset; break;
    case FILE_LSEEK_SUB:
        file->offset = (lseek_offset > file->offset) ? 0 : file->offset - lseek_offset;
        break;
    default: return (uint32)-1;
    }

    if (file->offset > ip->disk_info.size)
        file->offset = ip->disk_info.size;
    return file->offset;
}

file_t* file_dup(file_t* file)
{
    spinlock_acquire(&lk_file_table);
    file->ref++;
    spinlock_release(&lk_file_table);
    return file;
}

uint32 file_get_stat(file_t* file, uint64 user_dst)
{
    inode_t* ip = file->ip;
    file_stat_t stat;
    stat.type = ip->disk_info.type;
    stat.nlink = ip->disk_info.nlink;
    stat.size = ip->disk_info.size;
    stat.inode_num = ip->inode_num;
    stat.offset = file->offset;

    proc_t* p = myproc();
    uvm_copyout(p->pgtbl, user_dst, (uint64)&stat, sizeof(stat));
    return 0;
}

static void sb_print()
{
    printf("\ndisk layout information:\n");
    printf("1. super block:  block[%d]\n", FS_SB_BLOCK);
    printf("2. inode bitmap: block[%d - %d]\n", sb.inode_bitmap_firstblock,
        sb.inode_bitmap_firstblock + sb.inode_bitmap_blocks - 1);
    printf("3. inode region: block[%d - %d]\n", sb.inode_firstblock,
        sb.inode_firstblock + sb.inode_blocks - 1);
    printf("4. data bitmap:  block[%d - %d]\n", sb.data_bitmap_firstblock,
        sb.data_bitmap_firstblock + sb.data_bitmap_blocks - 1);
    printf("5. data region:  block[%d - %d]\n", sb.data_firstblock,
        sb.data_firstblock + sb.data_blocks - 1);
    printf("block size = %d Byte, total size = %d MB, total inode = %d\n\n", sb.block_size,
        (int)((unsigned long long)(sb.total_blocks) * sb.block_size / 1024 / 1024), sb.total_inodes);
}

void fs_init()
{
    buffer_init();

    buffer_t* buf = buffer_get(FS_SB_BLOCK);
    memmove(&sb, buf->data, sizeof(super_block_t));
    buffer_put(buf);

    if (sb.magic_num != FS_MAGIC)
        panic("fs_init: invalid magic number");

    sb_print();
    inode_init();
    file_init();
    device_init();

    // 确保根目录有效
    {
        inode_t* rooti = inode_get(ROOT_INODE);
        inode_lock(rooti);
        if (rooti->disk_info.index[0] == 0) {
            rooti->disk_info.index[0] = bitmap_alloc_block();
            buffer_t* rbuf = buffer_get(rooti->disk_info.index[0]);
            memset(rbuf->data, 0, BLOCK_SIZE);
            dentry_t* rde = (dentry_t*)(rbuf->data);
            rde[0].inode_num = ROOT_INODE; memmove(rde[0].name, ".", 2);
            rde[1].inode_num = ROOT_INODE; memmove(rde[1].name, "..", 3);
            rooti->disk_info.size = 2 * sizeof(dentry_t);
            buffer_write(rbuf); buffer_put(rbuf);
            inode_rw(rooti, true);
        }
        inode_unlock(rooti); inode_put(rooti);
    }

    // 创建设备文件
    if (path_to_inode("/dev") == NULL) {
        inode_t* ip = path_create_inode("/dev", INODE_TYPE_DIR,
            INODE_MAJOR_DEFAULT, INODE_MINOR_DEFAULT);
        if (ip) inode_put(ip);
    }
    static char* devs[] = { "/dev/stdin","/dev/stdout","/dev/stderr",
                           "/dev/zero","/dev/null","/dev/gpt0" };
    static uint16 majors[] = { INODE_MAJOR_STDIN,INODE_MAJOR_STDOUT,INODE_MAJOR_STDERR,
                              INODE_MAJOR_ZERO,INODE_MAJOR_NULL,INODE_MAJOR_GPT0 };
    for (int i = 0; i < 6; i++) {
        if (path_to_inode(devs[i]) == NULL) {
            inode_t* ip = path_create_inode(devs[i], INODE_TYPE_DIVICE,
                majors[i], INODE_MINOR_DEFAULT);
            if (ip) inode_put(ip);
        }
    }
}
