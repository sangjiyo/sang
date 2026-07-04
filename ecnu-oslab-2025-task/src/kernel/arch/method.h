#pragma once
#include "type.h"

/* RISC-V相关的寄存器读写 */

// 获取当前CPU的hartid
static inline uint64 r_mhartid()
{
    uint64 x;
    asm volatile("csrr %0, mhartid" : "=r"(x));
    return x;
}

// 读取mstatus寄存器
static inline uint64 r_mstatus()
{
    uint64 x;
    asm volatile("csrr %0, mstatus" : "=r"(x));
    return x;
}

// 写入mstatus寄存器
static inline void w_mstatus(uint64 x)
{
    asm volatile("csrw mstatus, %0" : : "r"(x));
}

// M-mode发生异常时，返回地址存在mepc寄存器
// 写入mepc寄存器
static inline void w_mepc(uint64 x)
{
    asm volatile("csrw mepc, %0" : : "r"(x));
}

// 读取sstatus寄存器
static inline uint64 r_sstatus()
{
    uint64 x;
    asm volatile("csrr %0, sstatus" : "=r"(x));
    return x;
}

// 写入sstatus寄存器
static inline void w_sstatus(uint64 x)
{
    asm volatile("csrw sstatus, %0" : : "r"(x));
}

// 读取sip寄存器
static inline uint64 r_sip()
{
    uint64 x;
    asm volatile("csrr %0, sip" : "=r"(x));
    return x;
}

// 写入sip寄存器
static inline void w_sip(uint64 x)
{
    asm volatile("csrw sip, %0" : : "r"(x));
}

// 读取sie寄存器
static inline uint64 r_sie()
{
    uint64 x;
    asm volatile("csrr %0, sie" : "=r"(x));
    return x;
}

// 写入sie寄存器
static inline void w_sie(uint64 x)
{
    asm volatile("csrw sie, %0" : : "r"(x));
}

// 读取mie寄存器
static inline uint64 r_mie()
{
    uint64 x;
    asm volatile("csrr %0, mie" : "=r"(x));
    return x;
}

// 写入mie寄存器
static inline void w_mie(uint64 x)
{
    asm volatile("csrw mie, %0" : : "r"(x));
}

// S-mode发生异常时，返回地址存在mepc寄存器
// 写入sepc寄存器
static inline void w_sepc(uint64 x)
{
    asm volatile("csrw sepc, %0" : : "r"(x));
}

// 读取sepc寄存器
static inline uint64 r_sepc()
{
    uint64 x;
    asm volatile("csrr %0, sepc" : "=r"(x));
    return x;
}

// 读取mdeleg寄存器
static inline uint64 r_medeleg()
{
    uint64 x;
    asm volatile("csrr %0, medeleg" : "=r"(x));
    return x;
}

// 写入mdeleg寄存器
static inline void w_medeleg(uint64 x)
{
    asm volatile("csrw medeleg, %0" : : "r"(x));
}

// 读取mideleg寄存器
static inline uint64 r_mideleg()
{
    uint64 x;
    asm volatile("csrr %0, mideleg" : "=r"(x));
    return x;
}

// 写入mideleg寄存器
static inline void w_mideleg(uint64 x)
{
    asm volatile("csrw mideleg, %0" : : "r"(x));
}

// 写入stvec寄存器
static inline void w_stvec(uint64 x)
{
    asm volatile("csrw stvec, %0" : : "r"(x));
}

// 读取stvec寄存器
static inline uint64 r_stvec()
{
    uint64 x;
    asm volatile("csrr %0, stvec" : "=r"(x));
    return x;
}

// 写入w_mtvec寄存器
static inline void w_mtvec(uint64 x)
{
    asm volatile("csrw mtvec, %0" : : "r"(x));
}

// 写入satp寄存器
static inline void w_satp(uint64 x)
{
    asm volatile("csrw satp, %0" : : "r"(x));
}

// 读取satp寄存器
static inline uint64 r_satp()
{
    uint64 x;
    asm volatile("csrr %0, satp" : "=r"(x));
    return x;
}

// 写入sscratch寄存器
static inline void w_sscratch(uint64 x)
{
    asm volatile("csrw sscratch, %0" : : "r"(x));
}

// 写入mscratch寄存器
static inline void w_mscratch(uint64 x)
{
    asm volatile("csrw mscratch, %0" : : "r"(x));
}

// 读取r_scause寄存器
static inline uint64 r_scause()
{
    uint64 x;
    asm volatile("csrr %0, scause" : "=r"(x));
    return x;
}

// 读取r_stval寄存器
static inline uint64 r_stval()
{
    uint64 x;
    asm volatile("csrr %0, stval" : "=r"(x));
    return x;
}

// 写入mcounteren寄存器
static inline void w_mcounteren(uint64 x)
{
    asm volatile("csrw mcounteren, %0" : : "r"(x));
}

// 读取mcountern寄存器
static inline uint64 r_mcounteren()
{
    uint64 x;
    asm volatile("csrr %0, mcounteren" : "=r"(x));
    return x;
}

// 读取时钟信息
static inline uint64 r_time()
{
    uint64 x;
    asm volatile("csrr %0, time" : "=r"(x));
    return x;
}

// 打开设备中断
static inline void intr_on()
{
    w_sstatus(r_sstatus() | SSTATUS_SIE);
}

// 关闭设备中断
static inline void intr_off()
{
    w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

// 设备中断是否打开
static inline int intr_get()
{
    uint64 x = r_sstatus();
    return (x & SSTATUS_SIE) != 0;
}

// 读取sp寄存器
static inline uint64 r_sp()
{
    uint64 x;
    asm volatile("mv %0, sp" : "=r"(x));
    return x;
}

// 读取tp寄存器（存储hartid）
static inline uint64 r_tp()
{
    uint64 x;
    asm volatile("mv %0, tp" : "=r"(x));
    return x;
}

// 写入tp寄存器
static inline void w_tp(uint64 x)
{
    asm volatile("mv tp, %0" : : "r"(x));
}

// 读取ra寄存器
static inline uint64 r_ra()
{
    uint64 x;
    asm volatile("mv %0, ra" : "=r"(x));
    return x;
}

// 刷新TLB (页表切换使用)
static inline void sfence_vma()
{
    asm volatile("sfence.vma zero, zero");
}
