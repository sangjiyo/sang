#include "mod.h"

// in trampoline.S
extern char trampoline[];  // 内核和用户切换的代码
extern char user_vector[]; // 用户触发陷阱进入内核
extern char user_return[]; // 内核处理完毕返回用户

// in trap.S
extern char kernel_vector[]; // 内核态trap处理流程, 进入内核后应当切换中断处理入口

// in trap_kernel.c
extern char* interrupt_info[16]; // 中断错误信息
extern char* exception_info[16]; // 异常错误信息

// 在user_vector()里面调用
// 用户态trap处理的核心逻辑
void trap_user_handler()
{

    printf("U");

    // 获取当前 CPU 上运行的进程（需确保 mycpu()->proc 已设置）
    proc_t* p = myproc();

    if (p == NULL)
        panic("trap_user_handler: no current process");

    // 进入内核态，立即将 stvec 设为内核向量，以正确处理内核态中断
    w_stvec((uint64)kernel_vector);

    trapframe_t* tf = p->tf;

    // 读取 scause, sepc 等
    uint64 scause = r_scause();
    uint64 sepc = r_sepc();
    uint64 stval = r_stval();

    // 保存用户 PC（用于可能的返回）
    tf->user_to_kern_epc = sepc;

    // 处理不同类型的 trap
    if (scause & 0x8000000000000000L) {
        // 中断（通常用户态中断会由内核处理，但也可直接处理）
        // 这里简单交给内核处理，但用户态中断较少，可忽略或转发
        panic("trap_user_handler: unexpected interrupt in user mode");
    }
    else {
        // 异常
        switch (scause) {
        case 8: {
            // 让 PC 跳过 ecall 指令（4 字节），避免循环
            tf->user_to_kern_epc += 4;

            // 从陷阱帧中读取系统调用号（a7 寄存器）
            uint64 syscall_num = tf->a7;

            // 开启中断，以便在处理系统调用时能响应其他中断
            intr_on();

            // 处理具体的系统调用
            if (syscall_num == 0) {
                printf("helloworld\n");
            }
            else {
                printf("Unknown syscall %d\n", syscall_num);
            }

            
            break;
        }
        default:
            printf("unexpected exception in user mode: scause=%p, sepc=%p, stval=%p\n",
                scause, sepc, stval);
            panic("trap_user_handler: unknown exception");
        }
    }

    // 返回用户态
    trap_user_return();
}

// 调用user_return()
// 内核态返回用户态
void trap_user_return()
{
    proc_t* p = myproc();

    // 关闭中断，保证设置过程原子性
    intr_off();

    //计算用户态 stvec
    uint64 user_stvec = TRAMPOLINE + ((uint64)user_vector - (uint64)trampoline);
    w_stvec(user_stvec);

    trapframe_t* tf = p->tf;

    tf->user_to_kern_satp = r_satp();
    tf->user_to_kern_sp = p->kstack + PGSIZE;
    tf->user_to_kern_trapvector = (uint64)trap_user_handler;
    tf->user_to_kern_hartid = r_tp();

    if (p == NULL)
        panic("trap_user_return: no current process");

    // 设置 sstatus 和 sepc，准备 sret
    uint64 sstatus = r_sstatus();
    // 清除 SPP（表示返回 User 模式）
    sstatus &= ~SSTATUS_SPP;
    // 开启用户态中断（若之前允许）
    sstatus |= SSTATUS_SPIE;
    w_sstatus(sstatus);
    w_sepc(tf->user_to_kern_epc);

    // 切换到用户页表并跳转到 user_return 汇编
    uint64 satp = MAKE_SATP(p->pgtbl);
    uint64 fn = TRAMPOLINE + (user_return - trampoline);

    // 调用 user_return(trapframe, pagetable)
    ((void (*)(uint64, uint64))fn)(TRAPFRAME, satp);

}