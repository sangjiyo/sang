#include "mod.h"
#include "../../user/initcode.h"

#define initcode target_user_initcode
#define initcode_len target_user_initcode_len

// in trampoline.S
extern char trampoline[];

// in swtch.S
extern void swtch(context_t *old, context_t *new);

// in trap/trap_user.c
extern void trap_user_return();

/* ------------本地变量----------- */

// 进程结构体数组 + 第一个用户进程的指针
static proc_t proc_list[N_PROC];
static proc_t *proczero;

// 全局pid + 保护它的锁
static int global_pid;
static spinlock_t pid_lk;

/* 获取一个pid */
static int alloc_pid()
{
    int tmp = 0;
    spinlock_acquire(&pid_lk);
    assert(global_pid > 0, "alloc_pid: overflow");
    tmp = global_pid++;
    spinlock_release(&pid_lk);
    return tmp;
}

/* 释放进程锁 + trap_user_return */
static void proc_return()
{

}

/* 进程模块初始化 */
void proc_init()
{    

}

/* 
    申请一个UNUSED进程结构体(返回时带锁)
    并执行通用的初始化逻辑
*/
proc_t *proc_alloc()
{

}

/* 
    回收一个进程结构体并释放它包含的资源
    tips: 调用者需要持有进程锁
*/
void proc_free(proc_t *p)
{

}

// 第一个用户进程
static proc_t proczero;

// 获得一个初始化过的用户页表
// 完成trapframe和trampoline的映射
pgtbl_t proc_pgtbl_init(uint64 trapframe)
{
    // 创建空页表（根页表）
    pgtbl_t pgtbl = (pgtbl_t)pmem_alloc(true);
    memset(pgtbl, 0, PGSIZE);

    // 映射 trampoline（用户态不可直接访问，但执行时需要）
    vm_mappages(pgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

    // 映射 trapframe（用户态需要读写）
    vm_mappages(pgtbl, TRAPFRAME, (uint64)trapframe, PGSIZE, PTE_R | PTE_W);

    return pgtbl;
}

/*
    第一个用户态进程的创建
    它的代码和数据位于initcode.h的initcode数组

    第一个进程的用户地址空间布局:
    trapoline   (1 page)
    trapframe   (1 page)
    ustack      (1 page)
    .......
                        <--heap_top
    code + data (1 page)
    empty space (1 page) 最低的4096字节 不分配物理页，同时不可访问

	注意: 用用户空间的地址映射需要标记 PTE_U
*/
void proc_make_first()
{
    proc_t* p = &proczero;

    //初始化mmap字段
    p->mmap = NULL;

    // 1. 设置pid
    p->pid = 0;

    // 2. 从用户区申请trapframe物理页
    p->tf = (trapframe_t*)pmem_alloc(false);

    // 3. 创建用户页表（映射trampoline + trapframe）
    p->pgtbl = proc_pgtbl_init((uint64)p->tf);

    // 4. 为ELF文件(code + data)申请一个物理页、进行数据转移、完成映射
    //    映射到USER_BASE (0x1000)，紧接在空页面(0x0000~0x0FFF)之上
    uint64 code_pa = (uint64)pmem_alloc(false);
    memmove((void*)code_pa, initcode, initcode_len);
    vm_mappages(p->pgtbl, USER_BASE, code_pa, PGSIZE, PTE_R | PTE_X | PTE_U);

    // 5. 申请用户栈的物理页并完成映射
    //    用户栈放在 USER_BASE + 2*PGSIZE 位置（中间预留1页作为guard）
    uint64 ustack_va = USER_BASE + 64 * PGSIZE;
    uint64 ustack_pa = (uint64)pmem_alloc(false);
    vm_mappages(p->pgtbl, ustack_va, ustack_pa, PGSIZE, PTE_R | PTE_W | PTE_U);
    p->ustack_npage = 1;

    // 6. 设置堆顶：代码段之后（栈之前）的区域是可扩展的堆空间
    p->heap_top = USER_BASE + PGSIZE;

    // 7. 设置trapframe中的用户程序入口和栈指针
    p->tf->user_to_kern_epc = USER_BASE;   // 用户代码起始地址
    p->tf->sp = ustack_va + PGSIZE;        // 用户栈指针（栈顶，向下生长）

    // 8. 内核栈相关设置
    //    KSTACK(0)已经在kvm_init中分配并映射好了
    p->kstack = KSTACK(0);

    // 9. 设置内核上下文（用于swtch切换）
    memset(&p->ctx, 0, sizeof(p->ctx));
    p->ctx.ra = (uint64)trap_user_return;  // swtch后跳转到trap_user_return
    p->ctx.sp = p->kstack + PGSIZE;        // 内核栈顶

    // 10. 把当前CPU执行的进程设为proczero
    mycpu()->proc = p;

    // 11. 通过swtch完成上下文切换
    //     "旧执行流"是内核自身(main) -> 保存到mycpu()->ctx
    //     "新执行流"是proczero -> 从p->ctx恢复
    swtch(&mycpu()->ctx, &p->ctx);
}

/*
    父进程产生子进程
    UNUSED -> RUNNABLE
*/
int proc_fork()
{

}

/*
    进程主动放弃CPU控制权
    RUNNING->RUNNABLE
*/
void proc_yield()
{

}

/*
    当父进程退出时, 让它的所有子进程认proczero为父
    因为proczero永不退出, 可以回收子进程的资源
*/
static void proc_reparent(proc_t *parent)
{

}

/*
    唤醒等待呼叫的进程
    由proc_exit调用
    tips: 调用者需要持有p的进程锁
*/
static void proc_try_wakeup(proc_t *p)
{

}

/*
    进程退出
    RUNNING -> ZOMBIE
*/
void proc_exit(int exit_code)
{

}

/*
    父进程等待一个子进程进入ZOMBIE状态
    1. 如果等到: 释放子进程, 返回子进程的pid, 将子进程的退出状态传出到user_addr
    2. 如果发现没孩子: 返回-1
    3. 如果没等到: 父进程进入睡眠状态 
*/
int proc_wait(uint64 user_addr)
{

}

/*
    进程等待sleep_space对应的资源, 进入睡眠状态
    RUNNING -> SLEEPING
*/
void proc_sleep(void *sleep_space, spinlock_t *lock)
{

}

/*
    唤醒所有等待sleep_space的进程
    SLEEPING -> RUNNABLE
*/
void proc_wakeup(void *sleep_space)
{

}

/* 
    用户进程切换到调度器
    tips: 调用者保证持有当前进程的锁
*/
void proc_sched()
{

}

/* 
    调度器
    RUNNABLE->RUNNING
*/
void proc_scheduler()
{

}