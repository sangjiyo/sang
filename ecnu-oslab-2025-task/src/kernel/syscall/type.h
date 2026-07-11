#pragma once
#include "../arch/type.h"

/* 系统调用号 */

#define SYS_brk 1               // 调整堆边界
#define SYS_mmap 2              // 创建内存映射
#define SYS_munmap 3            // 解除内存映射
#define SYS_fork 4              // 进程复制
#define SYS_wait 5              // 等待子进程退出
#define SYS_exit 6              // 进程退出
#define SYS_sleep 7             // 进程睡眠一段时间
#define SYS_getpid 8            // 获取当前进程的ID
#define SYS_exec 9              // 执行ELF文件
#define SYS_open 10             // 打开文件
#define SYS_close 11            // 关闭文件
#define SYS_read 12             // 读取文件
#define SYS_write 13            // 写入文件
#define SYS_lseek 14            // 移动读写指针
#define SYS_dup 15              // 复制文件权限
#define SYS_fstat 16            // 获取文件状态信息
#define SYS_get_dentries 17     // 获取目录下所有有效目录项
#define SYS_mkdir 18            // 创建目录文件
#define SYS_chdir 19            // 切换工作目录
#define SYS_print_cwd 20        // 打印工作目录的绝对路径
#define SYS_link 21             // 建立硬链接
#define SYS_unlink 22           // 解除硬链接

#define SYS_MAX_NUM 22

/* 可以传入的最大字符串长度 */
#define STR_MAXLEN 127