#include "help.h"
#include <stdarg.h>

/* helper function */

void memset(void *begin, uint8 data, uint32 n)
{
    uint8 *list = (uint8 *)begin;
    for (uint32 i = 0; i < n; i++)
        list[i] = data;
}

void memmove(void *dst, const void *src, uint32 n)
{
    char *d = dst;
    const char *s = src;
    while (n--)
    {
        *d = *s;
        d++;
        s++;
    }
}

int strncmp(const char *p, const char *q, uint32 n)
{
    while (n > 0 && *p && *p == *q)
        n--, p++, q++;
    if (n == 0)
        return 0;
    return (uint8)*p - (uint8)*q;
}

int strlen(const char *str)
{
  int i = 0;
  for (i = 0; str[i] != '\0'; i++)
    ;
  return i;
}

/* stdin stdout stderr */

uint32 stdin(char *str, uint32 len)
{
	return sys_read(STDIN, len, str);
}

uint32 stdout(char *str, uint32 len)
{
	return sys_write(STDOUT, len, str);
}

uint32 stderr(char *str, uint32 len)
{
	return sys_write(STDERR, len, str);
}

/* 格式化输出 fprintf */

static char digits[] = "0123456789ABCDEF";

static void printint(uint32 fd, int xx, int base, int sign)
{
    char buf[16 + 1];
    int i;
    uint32 x;

    if (sign && (sign = xx < 0))
        x = -xx;
    else
        x = xx;

    buf[16] = 0;
    i = 15;
    do
    {
        buf[i--] = digits[x % base];
    } while ((x /= base) != 0);

    if (sign)
        buf[i--] = '-';
    i++;
    if (i < 0)
        sys_write(fd, 14, "printint error");
    sys_write(fd, 16 - i, buf + i);
}

static void printptr(uint32 fd, uint64 x)
{
    int i = 0, j;
    char buf[32 + 1];
    buf[i++] = '0';
    buf[i++] = 'x';
    for (j = 0; j < (sizeof(uint64) * 2); j++, x <<= 4)
        buf[i++] = digits[x >> (sizeof(uint64) * 8 - 4)];
    buf[i] = 0;
    sys_write(fd, i, buf);
}

void fprintf(uint32 fd, const char *fmt, ...)
{
    va_list ap;
    int l = 0;
    char *a, *z, *s = (char *)fmt;
	char c;

    va_start(ap, fmt);
    for (;;)
    {
        if (!*s)
            break;
        for (a = s; *s && *s != '%'; s++)
            ;
        for (z = s; s[0] == '%' && s[1] == '%'; z++, s += 2)
            ;
        l = z - a;
        sys_write(fd, l, a);
        if (l)
            continue;
        if (s[1] == 0)
            break;
        switch (s[1])
        {
        case 'd':
            printint(fd, va_arg(ap, int), 10, 1);
            break;
        case 'x':
            printint(fd, va_arg(ap, int), 16, 1);
            break;
        case 'p':
            printptr(fd, va_arg(ap, uint64));
            break;
		case 'c':
			c = va_arg(ap, int);
			sys_write(fd, 1, &c);
			break;
        case 's':
            if ((a = va_arg(ap, char*)) == 0)
                a = "(null)";
            l = strlen(a);
            sys_write(fd, l, a);
            break;
        default:
            sys_write(fd, 1, "%");
            sys_write(fd, 1, s + 1);
            break;
        }
        s += 2;
    }
    va_end(ap);
}

/* 辅助输出dentry和fstat */

char *file_type[] = {"data", "dir", "device"};

void print_fstat(file_stat_t *stat, char *filename)
{
    fprintf(STDOUT, "file %s: type = %s, inum = %d, nlink = %d, size = %d, offset = %d\n",
        filename, file_type[stat->type], stat->inode_num, (uint32)(stat->nlink), stat->size, stat->offset);
}

void print_dentries(dentry_t* de, uint32 len, char *filename)
{
    fprintf(STDOUT, "directory %s:\n", filename);
    for (uint32 i = 0; i < len / sizeof(dentry_t); i++)
        fprintf(STDOUT, "dentry %d: inum = %d, name = %s\n", i, de[i].inode_num, de[i].name);
}