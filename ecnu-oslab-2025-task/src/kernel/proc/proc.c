#include "mod.h"
#include "../../user/initcode.h"

#define initcode target_user_initcode
#define initcode_len target_user_initcode_len

// in trampoline.S
extern char trampoline[];

// in swtch.S
extern void swtch(context_t* old, context_t* new);

// in trap/trap_user.c
extern void trap_user_return();

/* ------------本地变量----------- */

// 进程结构体数组 + 第一个用户进程的指针
static proc_t proc_list[N_PROC];
static proc_t* proczero;

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
    proc_t* p = myproc();

    // 释放由 proc_scheduler 获取的进程锁
    // 必须在fs_init之前释放, 否则磁盘I/O期间发生时钟中断时,
    // proc_yield会尝试再次获取p->lk导致递归panic
    spinlock_release(&p->lk);

    // 文件系统初始化: 只在第一次进入时执行 (由proczero完成)
    // 必须在用户进程上下文中执行 (因为可能触发proc_sleep)
    {
        static bool fs_inited = false;
        if (!fs_inited) {
            fs_inited = true;
            fs_init();
        }
    }

    // 跳转到用户态
    trap_user_return();
}

/* 进程模块初始化 */
void proc_init()
{
    // 初始化全局pid锁和全局pid (从1开始, pid=0保留)
    spinlock_init(&pid_lk, "pid_lk");
    global_pid = 1;

    // 初始化进程数组: 所有进程初始状态为UNUSED, 初始化每个进程的自旋锁
    for (int i = 0; i < N_PROC; i++) {
        proc_t* p = &proc_list[i];
        p->state = UNUSED;
        p->pid = 0;
        p->name[0] = '\0';
        spinlock_init(&p->lk, "proc");
        p->parent = NULL;
        p->exit_code = 0;
        p->sleep_space = NULL;
        p->pgtbl = NULL;
        p->heap_top = 0;
        p->ustack_npage = 0;
        p->mmap = NULL;
        p->tf = NULL;
        // kstack 在 kvm_init 中已经映射, 这里设置虚拟地址
        p->kstack = KSTACK(i);
        memset(&p->ctx, 0, sizeof(p->ctx));
    }

    proczero = NULL;
}

/*
    申请一个UNUSED进程结构体(返回时带锁)
    并执行通用的初始化逻辑
*/
proc_t* proc_alloc()
{
    proc_t* p;

    // 扫描进程数组, 寻找UNUSED进程
    for (p = proc_list; p < &proc_list[N_PROC]; p++) {
        spinlock_acquire(&p->lk);
        if (p->state == UNUSED) {
            goto found;
        }
        else {
            spinlock_release(&p->lk);
        }
    }
    // 没有空闲进程
    return NULL;

found:
    // 分配pid
    p->pid = alloc_pid();

    // 分配trapframe物理页
    p->tf = (trapframe_t*)pmem_alloc(false);
    if (p->tf == NULL) {
        spinlock_release(&p->lk);
        return NULL;
    }
    memset((void*)p->tf, 0, sizeof(trapframe_t));

    // 创建用户页表（映射trampoline + trapframe）
    p->pgtbl = proc_pgtbl_init((uint64)p->tf);
    if (p->pgtbl == NULL) {
        pmem_free((uint64)p->tf, false);
        p->tf = NULL;
        spinlock_release(&p->lk);
        return NULL;
    }

    // 初始化mmap字段
    p->mmap = NULL;

    // 设置内核上下文 (swtch后跳转到proc_return)
    memset(&p->ctx, 0, sizeof(p->ctx));
    p->ctx.ra = (uint64)proc_return;
    p->ctx.sp = p->kstack + PGSIZE;

    // 进程名称初始化为空
    p->name[0] = '\0';

    // 其他字段初始化为0
    p->heap_top = 0;
    p->ustack_npage = 0;
    p->parent = NULL;
    p->exit_code = 0;
    p->sleep_space = NULL;

    return p;
}

/*
    回收一个进程结构体并释放它包含的资源
    tips: 调用者需要持有进程锁
*/
void proc_free(proc_t* p)
{
    // 释放trapframe
    if (p->tf != NULL) {
        pmem_free((uint64)p->tf, false);
        p->tf = NULL;
    }

    // 销毁用户页表
    if (p->pgtbl != NULL) {
        uvm_destroy_pgtbl(p->pgtbl);
        p->pgtbl = NULL;
    }

    // 重置字段
    p->pid = 0;
    p->name[0] = '\0';
    p->state = UNUSED;
    p->parent = NULL;
    p->exit_code = 0;
    p->sleep_space = NULL;
    p->heap_top = 0;
    p->ustack_npage = 0;
    p->mmap = NULL;
}

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
    // 通过proc_alloc申请proczero, 删除不必要的复制
    proc_t* p = proc_alloc();
    if (p == NULL)
        panic("proc_make_first: proc_alloc failed");

    proczero = p;

    // 1. 设置进程名称
    memmove(p->name, "proczero", 9);

    // 2. 为ELF文件(code + data)申请一个物理页、进行数据转移、完成映射
    //    映射到USER_BASE (0x1000)，紧接在空页面(0x0000~0x0FFF)之上
    uint64 code_pa = (uint64)pmem_alloc(false);
    memmove((void*)code_pa, initcode, initcode_len);
    vm_mappages(p->pgtbl, USER_BASE, code_pa, PGSIZE, PTE_R | PTE_X | PTE_U);

    // 3. 申请用户栈的物理页并完成映射
    //    用户栈放在 USER_BASE + 64*PGSIZE 位置（中间预留1页作为guard）
    uint64 ustack_va = USER_BASE + 64 * PGSIZE;
    uint64 ustack_pa = (uint64)pmem_alloc(false);
    vm_mappages(p->pgtbl, ustack_va, ustack_pa, PGSIZE, PTE_R | PTE_W | PTE_U);
    p->ustack_npage = 1;

    // 4. 设置堆顶：代码段之后（栈之前）的区域是可扩展的堆空间
    p->heap_top = USER_BASE + PGSIZE;

    // 5. 设置trapframe中的用户程序入口和栈指针
    p->tf->user_to_kern_epc = USER_BASE;   // 用户代码起始地址
    p->tf->sp = ustack_va + PGSIZE;        // 用户栈指针（栈顶，向下生长）

    // 6. 设置为RUNNABLE状态, 让调度器来选择执行
    p->state = RUNNABLE;

    // 7. 解锁 (调度器会获取锁)
    spinlock_release(&p->lk);
}

/*
    父进程产生子进程
    UNUSED -> RUNNABLE
*/
int proc_fork()
{
    proc_t* p = myproc();
    proc_t* np;

    // 申请一个新的进程结构体
    np = proc_alloc();
    if (np == NULL)
        return -1;

    // 复制父进程的用户内存 (页表管理的全部区域)
    uvm_copy_pgtbl(p->pgtbl, np->pgtbl, p->heap_top, p->ustack_npage, p->mmap);

    // 复制父进程的堆栈信息
    np->heap_top = p->heap_top;
    np->ustack_npage = p->ustack_npage;

    // 复制父进程的mmap链表 (深拷贝)
    mmap_region_t* src_mmap = p->mmap;
    mmap_region_t** dst_ptr = &np->mmap;
    while (src_mmap != NULL) {
        mmap_region_t* new_region = mmap_region_alloc();
        new_region->begin = src_mmap->begin;
        new_region->npages = src_mmap->npages;
        new_region->next = NULL;
        *dst_ptr = new_region;
        dst_ptr = &new_region->next;
        src_mmap = src_mmap->next;
    }

    // 复制trapframe (保存的用户寄存器状态)
    memmove((void*)np->tf, (const void*)p->tf, sizeof(trapframe_t));

    // 子进程的fork返回值为0 (通过设置a0寄存器)
    np->tf->a0 = 0;

    // 记录父子关系
    np->parent = p;

    // 复制进程名称
    memmove(np->name, p->name, sizeof(p->name));

    // 设置状态为RUNNABLE
    np->state = RUNNABLE;

    int pid = np->pid;

    // 释放子进程锁
    spinlock_release(&np->lk);

    return pid;
}

/*
    进程主动放弃CPU控制权
    RUNNING->RUNNABLE
*/
void proc_yield()
{
    proc_t* p = myproc();
    spinlock_acquire(&p->lk);
    p->state = RUNNABLE;
    proc_sched();
    spinlock_release(&p->lk);
}

/*
    当父进程退出时, 让它的所有子进程认proczero为父
    因为proczero永不退出, 可以回收子进程的资源
*/
static void proc_reparent(proc_t* parent)
{
    for (proc_t* pp = proc_list; pp < &proc_list[N_PROC]; pp++) {
        // 检查pp->parent是否等于parent
        // 这里不先获取pp->lk, 因为只有父进程会修改pp->parent
        // 而我们就是父进程
        if (pp->parent == parent) {
            spinlock_acquire(&pp->lk);
            pp->parent = proczero;
            spinlock_release(&pp->lk);
        }
    }
}

/*
    唤醒等待呼叫的进程
    由proc_exit调用
    tips: 调用者需要持有p的进程锁
*/
static void proc_try_wakeup(proc_t* p)
{
    // 确保调用者持有p的进程锁
    // 如果p正在睡眠且等待的资源是它自己 (即p在proc_wait中睡眠)
    if (p->state == SLEEPING && p->sleep_space == p) {
        p->state = RUNNABLE;
        // 提示性输出: 进程被唤醒
        printf("proc %d is wakeup!\n", p->pid);
    }
}

/*
    进程退出
    RUNNING -> ZOMBIE
*/
void proc_exit(int exit_code)
{
    proc_t* p = myproc();

    // proczero 永不退出
    if (p == proczero)
        panic("proc_exit: proczero exiting");

    // 把所有子进程过继给proczero
    // 先获取proczero的锁, 再唤醒它 (防止死锁)
    spinlock_acquire(&proczero->lk);
    proc_reparent(p);
    // 唤醒proczero (如果它在wait中睡眠)
    proc_try_wakeup(proczero);
    spinlock_release(&proczero->lk);

    // 获取父进程 (在获取自己的锁之前保存, 防止竞态)
    spinlock_acquire(&p->lk);
    proc_t* original_parent = p->parent;
    spinlock_release(&p->lk);

    // 按父子顺序获取锁: 先父后子
    spinlock_acquire(&original_parent->lk);
    spinlock_acquire(&p->lk);

    // 设置退出状态
    p->exit_code = exit_code;

    // 标记为ZOMBIE
    p->state = ZOMBIE;

    // 唤醒父进程 (如果它在proc_wait中睡眠)
    proc_try_wakeup(original_parent);

    spinlock_release(&original_parent->lk);

    // 进入调度器, 永不返回
    proc_sched();
    panic("proc_exit: should never reach here");
}

/*
    父进程等待一个子进程进入ZOMBIE状态
    1. 如果等到: 释放子进程, 返回子进程的pid, 将子进程的退出状态传出到user_addr
    2. 如果发现没孩子: 返回-1
    3. 如果没等到: 父进程进入睡眠状态
*/
int proc_wait(uint64 user_addr)
{
    proc_t* p = myproc();
    int havekids = 0;

    // 持有p->lk以防止丢失子进程exit()发送的唤醒信号
    spinlock_acquire(&p->lk);

    for (;;) {
        // 扫描进程数组, 寻找处于ZOMBIE状态的子进程
        havekids = 0;
        for (proc_t* np = proc_list; np < &proc_list[N_PROC]; np++) {
            // 这里不先获取np->lk, 因为只有父进程会修改np->parent
            // 而我们就是父进程
            if (np->parent == p) {
                spinlock_acquire(&np->lk);
                havekids = 1;
                if (np->state == ZOMBIE) {
                    // 找到一个ZOMBIE的子进程
                    int pid = np->pid;

                    // 将子进程的退出状态拷贝到用户空间
                    if (user_addr != 0) {
                        uvm_copyout(p->pgtbl, user_addr, (uint64)&np->exit_code, sizeof(np->exit_code));
                    }

                    // 释放子进程
                    proc_free(np);
                    spinlock_release(&np->lk);
                    spinlock_release(&p->lk);
                    return pid;
                }
                spinlock_release(&np->lk);
            }
        }

        // 如果没有孩子, 返回-1
        if (!havekids) {
            spinlock_release(&p->lk);
            return -1;
        }

        // 没有找到ZOMBIE子进程, 进入睡眠 (以自身为资源)
        // 注意: proc_sleep 会释放 p->lk 并在返回时重新获取
        proc_sleep(p, &p->lk);
    }
}

/*
    进程等待sleep_space对应的资源, 进入睡眠状态
    RUNNING -> SLEEPING
*/
void proc_sleep(void* sleep_space, spinlock_t* lock)
{
    proc_t* p = myproc();

    // 获取进程锁
    // 如果调用者传入的不是进程锁, 需要先获取进程锁再释放传入的锁
    // 这样做是为了原子性: 保证不会错过wakeup
    if (lock != &p->lk) {
        spinlock_acquire(&p->lk);
        spinlock_release(lock);
    }

    // 进入睡眠
    p->sleep_space = sleep_space;
    p->state = SLEEPING;

    // 提示性输出: 进程进入睡眠
    //printf("proc %d is sleeping!\n", p->pid);

    // 切换到调度器
    proc_sched();

    // 被唤醒后, 清理睡眠信息
    p->sleep_space = NULL;

    // 恢复锁的状态
    if (lock != &p->lk) {
        spinlock_release(&p->lk);
        spinlock_acquire(lock);
    }
}

/*
    唤醒所有等待sleep_space的进程
    SLEEPING -> RUNNABLE
*/
void proc_wakeup(void* sleep_space)
{
    for (proc_t* p = proc_list; p < &proc_list[N_PROC]; p++) {
        spinlock_acquire(&p->lk);
        if (p->state == SLEEPING && p->sleep_space == sleep_space) {
            p->state = RUNNABLE;
            // 提示性输出: 进程被唤醒
            //printf("proc %d is wakeup!\n", p->pid);
        }
        spinlock_release(&p->lk);
    }
}

/*
    用户进程切换到调度器
    tips: 调用者保证持有当前进程的锁
*/
void proc_sched()
{
    proc_t* p = myproc();
    cpu_t* c = mycpu();

    // 安全检查
    if (!spinlock_holding(&p->lk))
        panic("proc_sched: not holding p->lk");
    if (c->noff != 1)
        panic("proc_sched: locks (noff != 1)");
    if (p->state == RUNNING)
        panic("proc_sched: running");
    if (intr_get())
        panic("proc_sched: interruptible");

    // 保存当前进程上下文, 切换到CPU调度器上下文
    // swtch返回时, 意味着该进程被调度器再次选中
    swtch(&p->ctx, &c->ctx);
}

/*
    调度器
    RUNNABLE->RUNNING
*/
void proc_scheduler()
{
    cpu_t* c = mycpu();
    c->proc = NULL;

    for (;;) {
        // 开启中断, 避免外设因中断被屏蔽而无法工作
        intr_on();

        // 循环扫描进程数组, 寻找RUNNABLE状态的进程
        for (proc_t* p = proc_list; p < &proc_list[N_PROC]; p++) {
            spinlock_acquire(&p->lk);
            if (p->state == RUNNABLE) {
                // 选中该进程, 设为RUNNING
                p->state = RUNNING;
                c->proc = p;

                // 提示性输出: 进程被调度执行
                //printf("proc %d is running...\n", p->pid);

                // 切换到进程的内核上下文
                // swtch返回时, 该进程已经放弃CPU (通过proc_sched)
                swtch(&c->ctx, &p->ctx);

                // 进程已让出CPU, 清理CPU状态
                c->proc = NULL;
            }
            spinlock_release(&p->lk);
        }
    }
}
