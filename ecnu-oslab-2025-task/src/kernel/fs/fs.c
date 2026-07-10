#include "mod.h"

super_block_t sb; /* 超级块 */

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
    buffer_t *buf = buffer_get(FS_SB_BLOCK);
    memmove(&sb, buf->data, sizeof(super_block_t));
    buffer_put(buf);

    // 3. 验证魔数
    if (sb.magic_num != FS_MAGIC)
        panic("fs_init: invalid magic number");

    // 4. 输出磁盘布局信息 (for debug)
    sb_print();
}
