/*
    clear: 清屏
    使用 ANSI 转义序列
*/
#include "help.h"

void main(int argc, char *argv[])
{
    fprintf(STDOUT, "\033[2J\033[H");
    sys_exit(0);
}
