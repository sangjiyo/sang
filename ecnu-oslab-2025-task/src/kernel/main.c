#include "arch/mod.h"
#include "lib/mod.h"

// 全局标志：用于主核通知从核初始化已完成
volatile int started = 0;

int main()
{
    // 获取当前 CPU 的 hartid（由 start.c 保存在 tp 中）
    int cpu_id = mycpuid();

    if (cpu_id == 0) {
        // 主核（CPU0）负责初始化串口和 printf 锁
        print_init();

        // 打印启动信息
        printf("cpu %d is booting!\n", cpu_id);

        // 内存屏障，确保之前的初始化操作对其它核可见
        __sync_synchronize();

        // 通知从核初始化完成
        started = 1;
    }
    else {
        // 从核（CPU1）等待主核完成初始化
        while (!started) {
            // 空循环等待，可考虑加入 wfi 指令（但无中断时效果相同）
        }
        // 确保读到 started 的最新值
        __sync_synchronize();

        // 打印启动信息
        printf("cpu %d is booting!\n", cpu_id);
    }

    // 打印完成后进入死循环（后续可扩展为调度器）
    while (1) {
        // 可加入 wfi 指令降低功耗，但无中断时无影响
        // asm volatile("wfi");
    }

    return 0;
}