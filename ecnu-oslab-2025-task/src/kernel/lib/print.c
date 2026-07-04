/* 标准输出和报错机制 */

#include "mod.h"

static char digits[] = "0123456789abcdef";

/* printf的自旋锁 */
static spinlock_t print_lk;

/* 初始化uart + 初始化printf锁 */
void print_init(void)
{
    uart_init();
    spinlock_init(&print_lk, "printf");
}

/* %d %p */
static void printint(int xx, int base, int sign)
{
    char buf[16];
    int i;
    uint32 x;

    if (sign && (sign = xx < 0))
        x = -xx;
    else
        x = xx;

    i = 0;
    do
    {
        buf[i++] = digits[x % base];
    } while ((x /= base) != 0);

    if (sign)
        buf[i++] = '-';

    while (--i >= 0)
        uart_putc_sync(buf[i]);
}

/* %x */
static void printptr(uint64 x)
{
    uart_putc_sync('0');
    uart_putc_sync('x');
    for (int i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
        uart_putc_sync(digits[x >> (sizeof(uint64) * 8 - 4)]);
}

/*
    标准化输出, 需要支持:
    1. %d (32位有符号数,以10进制输出)
    2. %p (32位无符号数,以16进制输出)
    3. %x (64位无符号数,以0x开头的16进制输出)
    4. %c (单个字符)
    5. %s (字符串)
    提示: stdarg.h中的va_list中包括你需要的参数地址
*/
void printf(const char *fmt, ...)
{

}



/* 如果发生panic, UART的停止标志 */
volatile int panicked = 0;

/* 报错并终止输出 */
void panic(const char *s)
{
    printf("panic! %s\n", s);
    panicked = 1;
    while (1)
        ;
}

/* 如果不满足条件, 则调用panic */
void assert(bool condition, const char *warning)
{

}
