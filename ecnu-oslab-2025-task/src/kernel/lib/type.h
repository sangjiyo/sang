#pragma once
#include "../arch/type.h"
#include <stdarg.h>

// the UART control registers.
// some have different meanings for
// read vs write.
// see http://byterunner.com/16550.html

#define RHR 0 // receive holding register (for input bytes)
#define THR 0 // transmit holding register (for output bytes)
#define IER 1 // interrupt enable register
#define IER_TX_ENABLE (1 << 0)
#define IER_RX_ENABLE (1 << 1)
#define FCR 2 // FIFO control register
#define FCR_FIFO_ENABLE (1 << 0)
#define FCR_FIFO_CLEAR (3 << 1) // clear the content of the two FIFOs
#define ISR 2                   // interrupt status register
#define LCR 3                   // line control register
#define LCR_EIGHT_BITS (3 << 0)
#define LCR_BAUD_LATCH (1 << 7) // special mode to set baud rate
#define LSR 5                   // line status register
#define LSR_RX_READY (1 << 0)   // input is waiting to be read from RHR
#define LSR_TX_IDLE (1 << 5)    // THR can accept another character to send

// 读写寄存器的宏定义
#define Reg(reg) ((volatile unsigned char *)(UART_BASE + reg))
#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

// UART 相关
#define UART_BASE 0x10000000ul
#define UART_IRQ 10

typedef struct cpu
{
    int noff;   // 关中断的深度
    int origin; // 第一次关中断前的状态
} cpu_t;
