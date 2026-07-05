#include "../arch/mod.h"
#include "../trap/mod.h"

// 每个CPU在运行操作系统时需要一个初始的函数栈
__attribute__((aligned(16))) uint8 CPU_stack[4096 * NCPU];

extern void main();

void start()
{
    // 暂时不开启分页，使用物理地址
    w_satp(0);

    // 切换到S-mode后无法访问M-mode的寄存器
    // 所以需要将hartid存到可访问的寄存器tp
    int id = r_mhartid();
    w_tp(id);

    // 修改mstatus寄存器，假装上一个状态是S-mode
    uint64 status = r_mstatus();
    status &= ~MSTATUS_MPP_MASK;
    status |= MSTATUS_MPP_S;
    w_mstatus(status);

    // 将异常和中断委托给 S-mode
    w_medeleg(0xffff);
    w_mideleg(0xffff);
    w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

    // 初始化时钟中断（必须在进入 S-mode 前设置好 M-mode 的中断向量）
    timer_init();

    // 设置M-mode的返回地址
    w_mepc((uint64)main);

    // 触发状态迁移，回到上一个状态（M-mode->S-mode）
    asm volatile("mret");
}
