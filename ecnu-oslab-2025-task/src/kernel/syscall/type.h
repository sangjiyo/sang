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
#define SYS_alloc_block 11  // 从data_bitmap申请1个block (测试data_bitmap_alloc)
#define SYS_free_block 12   // 向data_bitmap释放1个block (测试data_bitmap_free)
#define SYS_alloc_inode 13  // 从inode_bitmap申请1个inode (测试inode_bitmap_alloc)
#define SYS_free_inode 14   // 向inode_bitmap释放1个inode (测试inode_bitmap_free)
#define SYS_show_bitmap 15  // 输出目标bitmap的状态
#define SYS_get_block 16    // 获取1个描述block的buffer (测试buffer_get)
#define SYS_read_block 17   // 将buf->data拷贝到用户空间
#define SYS_write_block 18  // 基于用户地址空间更新buffer->data并写入磁盘 (测试buffer_write)
#define SYS_put_block 19    // 释放1个描述block的buffer (测试buffer_put)
#define SYS_show_buffer 20  // 输出buffer链表的状态
#define SYS_flush_buffer 21 // 释放非活跃链表中buffer持有的物理内存资源 (测试buffer_freemem)

#define SYS_MAX_NUM 21

/* 可以传入的最大字符串长度 */
#define STR_MAXLEN 127