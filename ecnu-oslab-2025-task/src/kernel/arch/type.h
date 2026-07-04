#pragma once

/* 基本类型定义 */

typedef char int8;
typedef short int16;
typedef int int32;
typedef long long int64;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;
typedef unsigned long long reg;

typedef enum
{
    false = 0,
    true = 1
} bool;

#ifndef NULL
#define NULL ((void *)0)
#endif


/* OS 全局变量 */

#define NCPU 2 // 最大CPU数量


/* RISC-V 架构常量与宏定义 */

/* Machine Status Register (mstatus) */
#define MSTATUS_MPP_MASK (3L << 11)
#define MSTATUS_MPP_M (3L << 11)
#define MSTATUS_MPP_S (1L << 11)
#define MSTATUS_MPP_U (0L << 11)
#define MSTATUS_MIE (1L << 3)

/* Supervisor Status Register (sstatus) */
#define SSTATUS_SPP (1L << 8)
#define SSTATUS_SPIE (1L << 5)
#define SSTATUS_UPIE (1L << 4)
#define SSTATUS_SIE (1L << 1)
#define SSTATUS_UIE (1L << 0)

/* Supervisor Interrupt Enable (sie) */
#define SIE_SEIE (1L << 9) /* 外部中断 */
#define SIE_STIE (1L << 5) /* 定时器中断 */
#define SIE_SSIE (1L << 1) /* 软件中断 */
