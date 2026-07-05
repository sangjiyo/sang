#include "arch/mod.h"
#include "lib/mod.h"
#include "mem/mod.h"
#include "trap/mod.h"

// 全局标志：用于主核通知从核初始化已完成
volatile static int started = 0;

// 测试模式：1-滴答测试（打印 ticks），2-多核 di da 测试 ， 3-输入输出测试
#define TEST_MODE 3        

int main()
{
    int cpuid = r_tp();

    if (cpuid == 0) {

        print_init();
        pmem_init();
        kvm_init();
        kvm_inithart();

        // 初始化 trap 子系统（PLIC、timer 锁、中断向量）
        trap_kernel_init();
        trap_kernel_inithart();

        printf("cpu %d is booting!\n", cpuid);
        __sync_synchronize();
        started = 1;

#if TEST_MODE == 1
        // 滴答测试：主核轮询 ticks 变化并打印
        uint64 last_ticks = 0;
        while (1) {
            uint64 now = timer_get_ticks();
            if (now != last_ticks) {
                printf("ticks = %d\n", now);
                last_ticks = now;
            }
            // 简单忙等待，保证中断能正常响应
            for (volatile int i = 0; i < 100000; i++);
        }
#elif TEST_MODE == 2
        // di da 测试：每次 ticks 增加时打印当前 CPU 标识
        uint64 last_ticks = 0;
        while (1) {
            uint64 now = timer_get_ticks();
            if (now != last_ticks) {
                printf("cpu %d:di da\n", cpuid);
                last_ticks = now;
            }
            for (volatile int i = 0; i < 10000000; i++);
        }
#elif TEST_MODE == 3
        while (1){}
#endif

    }
    else {

        while (started == 0);
        __sync_synchronize();
        // ---------- 从核 trap 初始化 ----------
        trap_kernel_inithart();

        printf("cpu %d is booting!\n", cpuid);

#if TEST_MODE == 2
        // 从核也参与 di da 打印
        uint64 last_ticks = 0;
        while (1) {
            uint64 now = timer_get_ticks();
            if (now != last_ticks) {
                printf("cpu %d:di da\n", cpuid);
                last_ticks = now;
            }
            for (volatile int i = 0; i < 10000000; i++);
        }
#else

        // 从核可以进入低功耗循环或执行其他任务
        while (1) {
            // 可加入 wfi 指令降低功耗
            // asm volatile("wfi");
        }
#endif

    }
    return 0;
}