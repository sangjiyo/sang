#pragma once

/* print.c: 标准输出与错误处理函数 */

void print_init(void);
void printf(const char *fmt, ...);
void panic(const char *s);
void assert(bool condition, const char *warning);

/* console.c: 控制台 */

void cons_init();
uint32 cons_write(uint32 len, uint64 src, bool is_user_src);
uint32 cons_read(uint32 len, uint64 dst, bool is_user_dst);
void cons_edit(int c);

/* uart.c: UART驱动函数 */

void uart_init(void);
void uart_putc_sync(int c);
int uart_getc_sync(void);
void uart_intr(void);

/* cpu.c: 获得CPU信息 */

int mycpuid(void);
cpu_t *mycpu(void);
proc_t *myproc(void);

/* utils.c: 一些常用的工具函数 */

void memset(void *begin, uint8 data, uint32 n);
void memmove(void *dst, const void *src, uint32 n);
int strncmp(const char *p, const char *q, uint32 n);
int strlen(const char *str);
