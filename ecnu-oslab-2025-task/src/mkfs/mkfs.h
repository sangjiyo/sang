/* disk layout: [ super block | inode bitmap | inode | data bitmap | data ] */

typedef enum {
    false = 0,
    true = 1
} bool;

#ifndef NULL
#define NULL ((void *)0)
#endif

#define MIN(a, b)  ((a) < (b) ? (a) : (b))
#define MAX(a, b)  ((a) > (b) ? (a) : (b))


/*------------------------关于超级块(super block)-------------------*/

#define FS_MAGIC 0x12341234                  // 魔数的预期值

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


/*------------------------关于索引节点(inode)---------------------*/

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
#define INODE_BLOCK_INDEX_2  (10+2048)            // 一级间接映射 (8MB)
#define INODE_BLOCK_INDEX_3  (10+2048+1024*1024)  // 二级间接映射 (4GB)

/*
	关于单个文件的最大容量:
	
	基于addrs进行计算, 单个文件最大可达 4GB + 8MB + 40KB
	1. 10 * 4KB = 40KB
	2. 2 * (4KB / 4B) * 4KB = 8MB
	3. 1 * (4KB / 4B) * (4KB / 4B) * 4KB = 4GB

	考虑到size是unsigned int类型, 文件需要小于4GB
*/

/* 索引节点(64 Byte) */
typedef struct inode_disk {
    short type;                              // 文件类型
    short major;                             // 主设备号
    short minor;                             // 次设备号
    short nlink;                             // 链接数
    unsigned int size;                       // 文件数据长度(字节)
    unsigned int index[INODE_INDEX_3]; // 数据存储位置(10+2+1)
} inode_disk_t;


/*------------------------关于目录项(dentry)---------------------*/

#define MAXLEN_FILENAME 60            // 文件名的最大长度
#define INVALID_INODE_NUM 0xFFFFFFFF  // 无效inode_num

/* 目录项(64 Byte) */
typedef struct dentry {
    char name[MAXLEN_FILENAME];       // 文件名
    unsigned int inode_num;           // 索引节点序号
} dentry_t;


/*----------------------------其他定义---------------------------*/

// 文件系统常量定义
#define BLOCK_SIZE       4096                 // 块的大小与页面大小保持一致
#define N_DATA_BLOCK     (5 * 512 * 512)      // 数据区域设为 5GB
#define N_INODE          (1 << 16)            // 文件数量上限设为 65536个
#define ROOT_INODE_NUM   0                    // 根目录的inode序号

// 辅助计算
#define BIT_PER_BYTE      (8)
#define BIT_PER_BLOCK     (BLOCK_SIZE * BIT_PER_BYTE)
#define INODE_PER_BLOCK   (BLOCK_SIZE / sizeof(inode_disk_t))
#define DENTRY_PER_BLOCK  (BLOCK_SIZE / sizeof(dentry_t))
#define COUNT_BLOCKS(ele_num, ele_per_block)  (((ele_num) + (ele_per_block) - 1) / (ele_per_block)) 
