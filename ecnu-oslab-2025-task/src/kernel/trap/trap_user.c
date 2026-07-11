#include "mod.h"
#include "../syscall/mod.h"
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
            // 时钟中断处理后, 让当前进程主动让出CPU
            proc_yield();
            break;
        case 9:  // S-mode external interrupt（外设，如UART）
            external_interrupt_handler();
            break;
        default:
            printf("\nunexpected interrupt from user: %s\n", interrupt_info[trap_id]);
            printf("sepc = %x, stval = %x\n", sepc, stval);
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
            syscall();
            break;
        }
        case 12: // Instruction page fault
        case 13: // Load page fault
        case 15: // Store/AMO page fault
        {
            uint64 fault_addr = stval;
            uint64 ustack_va = USER_BASE + 64 * PGSIZE;
            uint64 current_low = ustack_va - (p->ustack_npage - 1) * PGSIZE;
            uint64 stack_high = ustack_va + PGSIZE;

            // 检查1：高于栈顶，非法（可能是堆或未映射区域）
            if (fault_addr >= stack_high) {
                printf("page fault at %x not in stack region\n", fault_addr);
                panic("trap_user_handler: unexpected page fault");
            }
            // 检查2：低于堆起始地址，非法（可能与代码/空页重叠）
            if (fault_addr < USER_BASE + PGSIZE) {
                printf("page fault at %x below heap region\n", fault_addr);
                panic("trap_user_handler: unexpected page fault (too low)");
            }
            // 检查3：已在已映射的栈区域内（不应缺页）
            if (fault_addr >= current_low) {
                printf("page fault at %x inside mapped stack\n", fault_addr);
                panic("trap_user_handler: fault in already mapped stack");
            }

            // 合法栈扩展
            uint64 old_npage = p->ustack_npage;
            uint64 new_npage = uvm_ustack_grow(p->pgtbl, old_npage, fault_addr);
            p->ustack_npage = new_npage;

            printf("page fault occured! trap id = %d\n", trap_id);
            printf("ustack_npage: %d -> %d\n", old_npage, new_npage);

            p->tf->user_to_kern_epc = sepc;  // 重新执行原指令
            break;
        }
        default:
            printf("\nunexpected exception from user: %s\n", exception_info[trap_id]);
            printf("trap_id = %d, sepc = %x, stval = %x\n", trap_id, sepc, stval);
            // 检查代码页(0x1000)和mmap页是否共享PA
            {
                pte_t* dp1 = vm_getpte(p->pgtbl, 0x1000, false);
                pte_t* dp2 = vm_getpte(p->pgtbl, MMAP_BEGIN, false);
                if (dp1 && dp2 && (*dp1 & PTE_V) && (*dp2 & PTE_V)) {
                    printf("code_pa=%x mmap_pa=%x %s\n",
                        PTE_TO_PA(*dp1), PTE_TO_PA(*dp2),
                        PTE_TO_PA(*dp1) == PTE_TO_PA(*dp2) ? "SAME!" : "diff");
                }
            }
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