#pragma once
#include "../lock/type.h"

// 同优先级的上下文
typedef struct context
{
    uint64 ra; // 返回地址
    uint64 sp; // 栈指针

    // callee-saved通用寄存器
    uint64 s0;
    uint64 s1;
    uint64 s2;
    uint64 s3;
    uint64 s4;
    uint64 s5;
    uint64 s6;
    uint64 s7;
    uint64 s8;
    uint64 s9;
    uint64 s10;
    uint64 s11;
} context_t;

// 跨优先级的上下文
typedef struct trapframe
{
    /*   0 */ uint64 user_to_kern_satp;        // kernel page table
    /*   8 */ uint64 user_to_kern_sp;          // top of process's kernel stack
    /*  16 */ uint64 user_to_kern_trapvector;  // usertrap()
    /*  24 */ uint64 user_to_kern_epc;         // saved user program counter
    /*  32 */ uint64 user_to_kern_hartid;      // saved kernel tp

    // 全部通用寄存器
    /*  40 */ uint64 ra;
    /*  48 */ uint64 sp;
    /*  56 */ uint64 gp;
    /*  64 */ uint64 tp;
    /*  72 */ uint64 t0;
    /*  80 */ uint64 t1;
    /*  88 */ uint64 t2;
    /*  96 */ uint64 s0;
    /* 104 */ uint64 s1;
    /* 112 */ uint64 a0;
    /* 120 */ uint64 a1;
    /* 128 */ uint64 a2;
    /* 136 */ uint64 a3;
    /* 144 */ uint64 a4;
    /* 152 */ uint64 a5;
    /* 160 */ uint64 a6;
    /* 168 */ uint64 a7;
    /* 176 */ uint64 s2;
    /* 184 */ uint64 s3;
    /* 192 */ uint64 s4;
    /* 200 */ uint64 s5;
    /* 208 */ uint64 s6;
    /* 216 */ uint64 s7;
    /* 224 */ uint64 s8;
    /* 232 */ uint64 s9;
    /* 240 */ uint64 s10;
    /* 248 */ uint64 s11;
    /* 256 */ uint64 t3;
    /* 264 */ uint64 t4;
    /* 272 */ uint64 t5;
    /* 280 */ uint64 t6;
} trapframe_t;

// 外部结构体
typedef uint64 *pgtbl_t;
typedef struct mmap_region mmap_region_t;
typedef struct inode inode_t;
typedef struct file file_t;

/*
    可能的进程状态转换：
    UNUSED -> RUNNABLE 进程初始化
    RUNNABLE -> RUNNIGN 进程获得CPU使用权
    RUNNING -> RUNNABLE 进程失去CPU使用权
    RUNNING -> SLEEPING 进程睡眠
    SLEEPING -> RUNNABLE 进程苏醒
    RUNNING -> ZOMBIE 进程宣布退出
    ZOMBIE -> UNUSED 进程被父进程回收
*/
enum proc_state
{
    UNUSED,   // 未被使用
    RUNNABLE, // 准备就绪
    RUNNING,  // 运行中
    SLEEPING, // 睡眠等待
    ZOMBIE,   // 濒临死亡
};

// 单个进程最多打开N_OPEN_FILE_PER_PROC个文件
#define N_OPEN_FILE_PER_PROC 10

#define PROC_NAME_LEN 16

// 进程
typedef struct proc
{
    int pid;               // 标识符
    char name[PROC_NAME_LEN]; // 进程名称

    spinlock_t lk;         // 自旋锁, 保护下面4个字段
    enum proc_state state; // 进程状态
    struct proc *parent;   // 父进程
    int exit_code;         // 进程退出状态(父进程关心)
    void *sleep_space;     // 进程睡眠位置(等待的资源)

    pgtbl_t pgtbl;         // 用户态页表
    uint64 heap_top;       // 用户堆顶(以字节为单位)
    uint64 ustack_npage;   // 用户栈占用的页面数量
    mmap_region_t *mmap;   // 用户态mmap区域
    trapframe_t *tf;       // 用户态内核态切换时的运行环境暂存空间

    uint64 kstack;         // 内核栈的虚拟地址
    context_t ctx;         // 内核态进程上下文

    inode_t *cwd;          // 工作目录
    file_t *open_file[N_OPEN_FILE_PER_PROC]; // 打开文件表

} proc_t;

// 系统中最多同时存在N_PROC个进程
#define N_PROC 32


// 关于elf文件的解析

/* 
    ELF文件的构成:
    elf header            全局元数据
    program header table  描述各个segments
    sections(segments)    从编译和执行两个角度
    section header table  描述各个sections
*/

#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian

typedef struct elf_header
{
    uint32 magic;          // 应该是ELF_MAGIC
    uint8  elf[12];        // 一些信息
    uint16 type;           // ELF文件类型(如可执行文件、共享库等)
    uint16 machine;        // 机器的指令集架构(如x86、risc-v等)
    uint32 version;        // ELF版本号
    uint64 entry;          // 程序入口地址
    uint64 ph_off;         // program header table偏移量
    uint64 sh_off;         // section header table偏移量
    uint32 flags;          // 处理器特定标志
    uint16 eh_size;        // elf_header本身的大小 
    uint16 ph_ent_size;    // program header table里每个entry的大小 
    uint16 ph_ent_num;     // program header table里的entry数量
    uint16 sh_ent_size;    // section header table里每个entry的大小
    uint16 sh_ent_num;     // section header table里的entry数量
    uint16 sh_str_ndx;     // section header table中包含节字符串表索引的entry的索引
} elf_header_t;

// program header
typedef struct program_header
{
    uint32 type;
    uint32 flags;
    uint64 off;
    uint64 va;
    uint64 pa;
    uint64 file_size;
    uint64 mem_size;
    uint64 align;
} program_header_t;

// program_header->type = load 时载入内存
#define ELF_PROG_LOAD        1

// program_header->flags取值
#define ELF_PROG_FLAG_EXEC   1
#define ELF_PROG_FLAG_WRITE  2
#define ELF_PROG_FLAG_READ   4

// 最大参数量
#define ELF_MAXARGS          32
// 单个参数长度限制
#define ELF_MAXARG_LEN       (PGSIZE / ELF_MAXARGS)
