#pragma once
#include "../arch/type.h"

/* 系统调用号 */

#define SYS_brk 1           // 调整堆边界
#define SYS_mmap 2          // 创建内存映射
#define SYS_munmap 3        // 解除内存映射
#define SYS_print_str 4     // 打印字符串
#define SYS_print_int 5     // 打印32位整数
#define SYS_getpid 6        // 获取当前进程的ID
#define SYS_fork 7          // 进程复制
#define SYS_wait 8          // 等待子进程退出
#define SYS_exit 9          // 进程退出
#define SYS_sleep 10        // 进程睡眠一段时间

#define SYS_MAX_NUM 10

/* 可以传入的最大字符串长度 */
#define STR_MAXLEN 127