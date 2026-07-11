/*
    echo: 回显参数
    用法: echo [args...]
*/
#include "help.h"

void main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (i > 1) fprintf(STDOUT, " ");
        fprintf(STDOUT, "%s", argv[i]);
    }
    fprintf(STDOUT, "\n");
    sys_exit(0);
}
