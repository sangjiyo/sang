#include "mod.h"
#include "../../user/syscall_num.h"

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

    // 确认trap来自U-mode
    uint64 sstatus = r_sstatus();
    assert(!(sstatus & SSTATUS_SPP), "trap_user_handler: not from user mode");

    // 进入内核后，将trap入口切换为kernel_vector
    // 这样在内核处理期间发生的trap由kernel_vector处理
    w_stvec((uint64)kernel_vector);

    uint64 sepc = r_sepc();       // 发生trap时的用户PC
    uint64 scause = r_scause();   // trap原因
    uint64 stval = r_stval();     // 附加信息

    // 获取当前 CPU 上运行的进程（需确保 mycpu()->proc 已设置）
    proc_t* p = myproc();

    // 保存用户PC到trapframe（用于后续返回到用户态）
    p->tf->user_to_kern_epc = sepc;

    /* 高位bit标识了是中断还是异常 */
    if (scause & 0x8000000000000000ul) {
        // 1-中断处理
        int trap_id = scause & 0xf;
        switch (trap_id)
        {
        case 1:  // S-mode software interrupt（时钟中断由M-mode转发）
            timer_interrupt_handler();
            break;
        case 9:  // S-mode external interrupt（外设，如UART）
            external_interrupt_handler();
            break;
        default:
            printf("\nunexpected interrupt from user: %s\n", interrupt_info[trap_id]);
            printf("sepc = %p, stval = %p\n", sepc, stval);
            panic("trap_user_handler: interrupt");
        }
    }
    else {
        // 2-异常处理
        int trap_id = scause & 0xf;
        switch (trap_id)
        {
        case 8:  // Environment call from U-mode (ecall) -> 系统调用
        {
            // 系统调用：epc指向ecall指令，返回时需跳过它，PC=PC+4
            p->tf->user_to_kern_epc += 4;

            // 系统调用号存放在a7寄存器中（RISC-V syscall约定）
            uint64 sys_num = p->tf->a7;
            if (sys_num == SYS_helloworld) {
                printf("proczero: hello world!\n");
            }
            else {
                printf("unknown syscall: %d from pid %d\n", sys_num, p->pid);
            }
            break;
        }

        default:
            printf("\nunexpected exception from user: %s\n", exception_info[trap_id]);
            printf("trap_id = %d, sepc = %p, stval = %p\n", trap_id, sepc, stval);
            panic("trap_user_handler: exception");
        }
    }

    // trap处理完毕，返回用户态
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