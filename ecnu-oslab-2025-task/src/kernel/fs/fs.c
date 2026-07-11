#include "mod.h"

super_block_t sb; /* 超级块 */

file_t file_table[N_FILE]; // 文件资源池
spinlock_t lk_file_table; // 保护它的锁

/* 初始化file_table */
void file_init()
{

}

/* 从file_table中获取1个空闲file */
file_t* file_alloc()
{

}

/*
	根据路径打开文件 (指定打开模式)
	成功返回file, 失败返回NULL
*/
file_t* file_open(char *path, uint32 open_mode)
{

}

/* 关闭文件 */
void file_close(file_t *file)
{

}

/* 读取文件内容, 返回读到的字节数量 */
uint32 file_read(file_t* file, uint32 len, uint64 dst, bool is_user_dst)
{

}

/* 读取文件内容, 返回读到的字节数量 */
uint32 file_write(file_t* file, uint32 len, uint64 src, bool is_user_src)
{

}

/* 
	读/写指针的移动
	对于不合理的lseek_offset, 只做尽力而为的移动
	返回新的file->offset
*/
uint32 file_lseek(file_t *file, uint32 lseek_offset, uint32 lseek_flag)
{

}

/* file->ref++ with lock protect */
file_t* file_dup(file_t* file)
{

}

/* 获取文件参数, 成功返回0, 失败返回-1 */
uint32 file_get_stat(file_t* file, uint64 user_dst)
{

}

/* 基于superblock输出磁盘布局信息 (for debug) */
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

/* 文件系统初始化 */
void fs_init()
{
    // 1. 初始化缓冲系统
    buffer_init();

    // 2. 读入超级块 (位于 block[0])
    buffer_t* buf = buffer_get(FS_SB_BLOCK);
    memmove(&sb, buf->data, sizeof(super_block_t));
    buffer_put(buf);

    // 3. 验证魔数
    if (sb.magic_num != FS_MAGIC)
        panic("fs_init: invalid magic number");

    // 4. 输出磁盘布局信息 (for debug)
    sb_print();

    // 5. 初始化inode缓存
    inode_init();
}
