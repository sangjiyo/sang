#include "mod.h"

// 中断信息
static char *interrupt_info[16] = {
    "U-mode software interrupt",      // 0
    "S-mode software interrupt",      // 1
    "reserved-1",                     // 2
    "M-mode software interrupt",      // 3
    "U-mode timer interrupt",         // 4
    "S-mode timer interrupt",         // 5
    "reserved-2",                     // 6
    "M-mode timer interrupt",         // 7
    "U-mode external interrupt",      // 8
    "S-mode external interrupt",      // 9
    "reserved-3",                     // 10
    "M-mode external interrupt",      // 11
    "reserved-4",                     // 12
    "reserved-5",                     // 13
    "reserved-6",                     // 14
    "reserved-7",                     // 15
};

// 异常信息
static char *exception_info[16] = {
    "Instruction address misaligned", // 0
    "Instruction access fault",       // 1
    "Illegal instruction",            // 2
    "Breakpoint",                     // 3
    "Load address misaligned",        // 4
    "Load access fault",              // 5
    "Store/AMO address misaligned",   // 6
    "Store/AMO access fault",         // 7
    "Environment call from U-mode",   // 8
    "Environment call from S-mode",   // 9
    "reserved-1",                     // 10
    "Environment call from M-mode",   // 11
    "Instruction page fault",         // 12
    "Load page fault",                // 13
    "reserved-2",                     // 14
    "Store/AMO page fault",           // 15
};

// 实现位于 trap.S
// 它是完整的内核态trap处理流程
extern void kernel_vector();

// 初始化trap中各个核心共享的东西
void trap_kernel_init()
{
    // PLIC初始化
    plic_init();

    // 系统时钟创建
    timer_create();
}

// 初始化trap中各个核心独有的东西
void trap_kernel_inithart()
{
    // PLIC核心初始化
    plic_inithart();

    // 填写内核态中断处理函数
    w_stvec((uint64)kernel_vector);

    // 打开中断
    intr_on();
}

// 在kernel_vector()里面调用
// 内核态trap处理的核心逻辑
void trap_kernel_handler()
{
    uint64 sepc = r_sepc();       // 记录了发生异常时的PC值
    uint64 sstatus = r_sstatus(); // 与特权模式和中断相关的状态信息
    uint64 scause = r_scause();   // 引发trap的原因
    uint64 stval = r_stval();     // 发生trap时保存的附加信息 (不同trap类型不一样)

    // 确认trap来自S-mode且此时trap处于关闭状态
    assert(sstatus & SSTATUS_SPP, "trap_kernel_handler: not from s-mode");
    assert(intr_get() == 0, "trap_kernel_handler: interreput enabled");

    int trap_id = scause & 0xf;

    /* 高位bit标识了是中断还是异常 */
    if (scause & 0x8000000000000000ul) {
        // 1-中断处理
        switch (trap_id) // 中断产生原因分类
        {

        default: // 例外处理
            printf("\nunexpected interrupt: %s\n", interrupt_info[trap_id]);
            printf("trap_id = %d, sepc = %p, stval = %p\n", trap_id, sepc, stval);
            panic("trap_kernel_handler");
        }
    } else {
        // 2-异常处理
        switch (trap_id) // 异常产生原因分类
        {

        default: // 例外处理
            printf("\nunexpected exception: %s\n", exception_info[trap_id]);
            printf("trap_id = %d, sepc = %p, stval = %p\n", trap_id, sepc, stval);
            panic("trap_kernel_handler");
        }
    }
}

// 外设中断处理 (基于PLIC，lab-3只需要识别和处理UART中断)
void external_interrupt_handler()
{

}

// 时钟中断处理 (基于CLINT)
void timer_interrupt_handler()
{
    // 由于sys_timer是共享资源, 但每个CPU都能收到时钟中断
    // 所以只需要指定一个CPU(CPU-0)负责更新时钟
    if(mycpuid() == 0)
        timer_update();
    // 清除 SSIP bit (S-mode software interrupt pending)
    // 宣布 S-mode 软件中断处理完成
    // 在 trap.S 里面有对应的两条命令, 去找找
    w_sip(r_sip() & ~2);
}