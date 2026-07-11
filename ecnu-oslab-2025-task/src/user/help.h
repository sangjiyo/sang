#ifndef __HELP_H__
#define __HELP_H__

/* 类型定义 */

typedef char                       int8;
typedef short                      int16;
typedef int                        int32;
typedef long long                  int64;
typedef unsigned char              uint8;
typedef unsigned short             uint16;
typedef unsigned int               uint32;
typedef unsigned long long         uint64;
typedef enum {false = 0, true = 1} bool;

#ifndef NULL
#define NULL ((void*)0)
#endif

/* 其他定义 */

#define PGSIZE 4096            // 内存页面大小
#define BLOCKSIZE 4096         // 磁盘块大小
#define MAXLEN_FILENAME 60     // 文件名最大长度
#define MAXLEN_STR 127         // 传入字符串/文件路径最大长度

#define VA_MAX (1ul << 38)                                 // 最大的虚拟地址
#define MMAP_END (VA_MAX - 2 * PGSIZE - 16 * 256 * PGSIZE) // mmap_region的最大地址
#define MMAP_BEGIN (MMAP_END - 64 * 256 * PGSIZE)          // mmap_region的最小地址

#define ELF_MAXARGS        32                              // exec的最大参数数量
#define ELF_MAXARG_LEN     (PGSIZE / ELF_MAXARGS)          // exec单个参数的最大长度

#define OPEN_CREATE 0x01       // 打开文件时, 若文件不存在则创建新的
#define OPEN_READ   0x02       // 打开文件时, 要求文件可读
#define OPEN_WRITE  0x04       // 打开文件时, 要求文件可写

#define LSEEK_SET     0        // file->offset = lseek_offset
#define LSEEK_ADD     1        // file->offset += lseek_offset
#define LSEEK_SUB     2        // file->offset -= lseek_offset

#define TYPE_DATA     0        // file管理无结构的流式数据
#define TYPE_DIR      1        // file管理结构化的目录数据
#define TYPE_DIVICE   2        // file对应虚拟设备(不管理数据)

#define STDIN         0        // 标准输入
#define STDOUT        1        // 标准输出
#define STDERR        2        // 标准错误输出

typedef struct dentry {
	char name[MAXLEN_FILENAME];
    uint32 inode_num;
} dentry_t;

typedef struct file_stat {
    uint16 type;        // inode_disk->type
    uint16 nlink;       // inode_disk->nlink
    uint32 size;        // inode_disk->size
    uint32 inode_num;   // inode->inode_num
    uint32 offset;      // file->offset
} file_stat_t;

/* 第一类: 系统调用函数 */

uint64 sys_brk(uint64 new_heap_top);
uint64 sys_mmap(uint64 start, uint32 len);
uint64 sys_munmap(uint64 start, uint32 len);
uint32 sys_fork();
uint32 sys_wait(uint32 *exit_state);
void   sys_exit(uint32 exit_state);
uint32 sys_sleep(uint32 ntick);
uint32 sys_getpid();
uint32 sys_exec(char *path, char **argv);
uint32 sys_open(char *path, uint32 open_mode);
uint32 sys_close(uint32 fd);
uint32 sys_read(uint32 fd, uint32 len, void *addr);
uint32 sys_write(uint32 fd, uint32 len, void *addr);
uint32 sys_lseek(uint32 fd, uint32 offset, uint32 flag);
uint32 sys_dup(uint32 fd);
uint32 sys_fstat(uint32 fd, file_stat_t *stat);
uint32 sys_get_dentries(uint32 fd, dentry_t *buf, uint32 buf_len);
uint32 sys_mkdir(char *path);
uint32 sys_chdir(char *new_path);
uint32 sys_print_cwd();
uint32 sys_link(char *old_path, char *new_path);
uint32 sys_unlink(char *path);

/* 第二类: 其他辅助函数 */

void memset(void *begin, uint8 data, uint32 n);
void memmove(void *dst, const void *src, uint32 n);
int strncmp(const char *p, const char *q, uint32 n);
int strlen(const char *str);
uint32 stdin(char *str, uint32 len);
uint32 stdout(char *str, uint32 len);
uint32 stderr(char *str, uint32 len);
void fprintf(uint32 fd, const char *fmt, ...);
void print_fstat(file_stat_t *stat, char *filename);
void print_dentries(dentry_t* de, uint32 len, char *filename);

#endif