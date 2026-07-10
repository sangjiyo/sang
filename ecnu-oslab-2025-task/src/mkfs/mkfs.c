#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include "mkfs.h"

static int disk_fd; // 磁盘映像的文件描述符
static super_block_t sb; // 超级块

// 转换成小端序
unsigned short xshort(unsigned short x)
{
    unsigned short y;
    unsigned char* a = (unsigned char*)&y;
    a[0] = x;
    a[1] = x >> 8;
    return y;
}

// 转换成小端序
unsigned int xint(unsigned int x)
{
    unsigned int y;
    unsigned char* a = (unsigned char*)&y;
    a[0] = x;
    a[1] = x >> 8;
    a[2] = x >> 16;
    a[3] = x >> 24;
    return y;
}

// buf -> block
void block_write(unsigned int block_num, void *buf)
{
    if(lseek(disk_fd, BLOCK_SIZE * block_num, 0) != BLOCK_SIZE * block_num) {
        perror("lsek");
        exit(1);
    }
    if(write(disk_fd, buf, BLOCK_SIZE) != BLOCK_SIZE) {
        perror("write");
        exit(1);
    }
}

// block -> buf
void block_read(unsigned int block_num, void *buf)
{
    if(lseek(disk_fd, BLOCK_SIZE * block_num, 0) != BLOCK_SIZE * block_num) {
        perror("lsek");
        exit(1);
    }
    if(write(disk_fd, buf, BLOCK_SIZE) != BLOCK_SIZE) {
        perror("read");
        exit(1);
    }
}

int main(int argc, char* argv[])
{
    assert(BLOCK_SIZE % sizeof(inode_disk_t) == 0);

	/* step-1: 填充 superblock 结构体 */
	sb.magic_num = xint(FS_MAGIC);
	sb.block_size = xint(BLOCK_SIZE);

	sb.inode_bitmap_firstblock = xint(1);
	sb.inode_bitmap_blocks = xint(COUNT_BLOCKS(N_INODE, BIT_PER_BLOCK));

	sb.inode_firstblock = xint(sb.inode_bitmap_firstblock + sb.inode_bitmap_blocks);
	sb.inode_blocks = xint(COUNT_BLOCKS(N_INODE, INODE_PER_BLOCK));

	sb.data_bitmap_firstblock = xint(sb.inode_firstblock + sb.inode_blocks);
	sb.data_bitmap_blocks = xint(COUNT_BLOCKS(N_DATA_BLOCK, BIT_PER_BLOCK));

	sb.data_firstblock = xint(sb.data_bitmap_firstblock + sb.data_bitmap_blocks);
	sb.data_blocks = xint(N_DATA_BLOCK);

    sb.total_inodes = xint(N_INODE);
	sb.total_blocks = xint(1 + sb.inode_bitmap_blocks + sb.inode_blocks
			+ sb.data_bitmap_blocks + sb.data_blocks);

    /* step-2: 创建磁盘文件 */
    disk_fd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
    if(disk_fd < 0) {
        perror(argv[1]);
        exit(1);
    }

    /* step-3: 准备一个清零的磁盘映像 */
    char buf[BLOCK_SIZE];
    memset(buf, 0, BLOCK_SIZE);

    printf("\nPreparing disk.img...\n\n");
    for(int i = 0; i < sb.total_blocks; i++)
        block_write(i, buf);

    /* step-4: 填写 super block */
    memmove(buf, &sb, sizeof(sb));
    block_write(0, buf);

    /* step-5: 关闭磁盘文件 */
    close(disk_fd);

    return 0;
}