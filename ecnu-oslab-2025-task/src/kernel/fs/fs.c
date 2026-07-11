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

    // ===== 测试代码 =====
    // 请根据需要选择测试1/2/3/4

			/* fs_init in fs.c */

	printf("============= test begin =============\n\n");

	inode_t* ip_1, * ip_2;
	uint32 len, cut_len;

	/* 小批量读写测试 */

	int small_src[10], small_dst[10];
	for (int i = 0; i < 10; i++)
		small_src[i] = i;

	ip_1 = inode_create(INODE_TYPE_DATA, INODE_MAJOR_DEFAULT, INODE_MINOR_DEFAULT);
	inode_lock(ip_1);
	inode_print(ip_1, "small_data");

	printf("writing data...\n\n");
	cut_len = 10 * sizeof(int);
	for (uint32 offset = 0; offset < 400 * cut_len; offset += cut_len) {
		len = inode_write_data(ip_1, offset, cut_len, small_src, false);
		assert(len == cut_len, "write fail 1!");
	}
	inode_print(ip_1, "small_data");

	len = inode_read_data(ip_1, 120 * cut_len + 4, cut_len, small_dst, false);
	assert(len == cut_len, "read fail 1!");
	printf("read data:");
	for (int i = 0; i < 10; i++)
		printf(" %d", small_dst[i]);
	printf("\n\n");

	ip_1->disk_info.nlink = 0;
	inode_unlock(ip_1);
	inode_put(ip_1);

	/* 大批量读写测试 */

	char* big_src, big_dst[9];
	big_dst[8] = 0;

	/* 申请五个连续物理页面 (初始化阶段, 通常来说能拿到连续的) */
	big_src = pmem_alloc(true);
	assert(pmem_alloc(true) == big_src + PGSIZE, "contiguous fail!");
	assert(pmem_alloc(true) == big_src + PGSIZE * 2, "contiguous fail!");
	assert(pmem_alloc(true) == big_src + PGSIZE * 3, "contiguous fail!");
	assert(pmem_alloc(true) == big_src + PGSIZE * 4, "contiguous fail!");

	for (uint32 i = 0; i < 5 * (PGSIZE / 8); i++)
		for (uint32 j = 0; j < 8; j++)
			big_src[i * 8 + j] = 'A' + j;

	ip_2 = inode_create(INODE_TYPE_DATA, INODE_MAJOR_DEFAULT, INODE_MINOR_DEFAULT);
	inode_lock(ip_2);
	inode_print(ip_2, "big_data");

	printf("writing data...\n\n");
	cut_len = PGSIZE * 4 + 1110;
	for (uint32 offset = 0; offset < cut_len * 10000; offset += cut_len)
	{
		len = inode_write_data(ip_2, offset, cut_len, big_src, false);
		assert(len == cut_len, "write fail 2!");
	}
	inode_print(ip_2, "big_data");

	len = inode_read_data(ip_1, cut_len * 10000 - 8, 8, big_dst, false);
	assert(len == 8, "read fail 2");
	printf("read data: %s\n", big_dst);


	ip_2->disk_info.nlink = 0;
	inode_unlock(ip_2);
	inode_put(ip_2);

	pmem_free((uint64)big_src, true);
	pmem_free((uint64)big_src + PGSIZE, true);
	pmem_free((uint64)big_src + PGSIZE * 2, true);
	pmem_free((uint64)big_src + PGSIZE * 3, true);
	pmem_free((uint64)big_src + PGSIZE * 4, true);


	printf("============= test end =============\n");

	while (1);
}
