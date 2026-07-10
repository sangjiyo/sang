#include "mod.h"

// 从begin开始对连续n个字节赋值data
void memset(void *begin, uint8 data, uint32 n)
{
    uint8 *list = (uint8 *)begin;
    for (uint32 i = 0; i < n; i++)
        list[i] = data;
}

// 从src向dst拷贝n个字节的数据
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

// 字符串p的前n个字符与q做比较
// 按照ASCII码大小逐个比较
// 相同返回0 大于或小于返回正数或负数
int strncmp(const char *p, const char *q, uint32 n)
{
    while (n > 0 && *p && *p == *q)
        n--, p++, q++;
    if (n == 0)
        return 0;
    return (uint8)*p - (uint8)*q;
}