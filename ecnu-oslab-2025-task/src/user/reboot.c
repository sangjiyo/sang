/*
    reboot: 重启系统
*/
#include "help.h"

void main(int argc, char *argv[])
{
    fprintf(STDOUT, "System is rebooting...\n");
    fprintf(STDOUT, "Please restart QEMU manually.\n");
    /* 没有真正的 reboot 系统调用, 简单退出 */
    sys_exit(0);
}
