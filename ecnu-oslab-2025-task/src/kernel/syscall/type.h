#pragma once
#include "../arch/type.h"

/* 系统调用号 */

#define SYS_copyin 1    // 用户->内核数据复制
#define SYS_copyout 2   // 内核->用户数据复制
#define SYS_copyinstr 3 // 用户->内核字符串复制
#define SYS_brk 4       // 调整堆边界
#define SYS_mmap 5      // 创建内存映射
#define SYS_munmap 6    // 解除内存映射

#define SYS_MAX_NUM 6

/* 可以传入的最大字符串长度 */
#define STR_MAXLEN 127