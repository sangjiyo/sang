#include "sys.h"

int main()
{
    char path[] = "./shell";
    char arg0[] = "shell";
    char *argv[] = {arg0, 0};

    char str_fork_fail[] = "initcode: fork fail!\n";
    char str_start[]    = "\n======== Booting ECNU-OS ========\n\n";
    char str_success[]  = "\n======== boot success  ========\n";
    char str_fail[]     = "\n======== boot fail     ========\n";
    char str_exec_fail[] = "initcode: exec shell fail!\n";

    int ret, pid = syscall(SYS_fork);

    if (pid < 0) {
        syscall(SYS_write, 1, sizeof(str_fork_fail), str_fork_fail);
    } else if (pid == 0) {
        syscall(SYS_write, 1, sizeof(str_start), str_start);
        ret = (int)syscall(SYS_exec, path, argv);
        if (ret != 0) {
            syscall(SYS_write, 1, sizeof(str_exec_fail), str_exec_fail);
            syscall(SYS_exit, 1);
        }
    } else {
        unsigned int exit_state = 0;
        syscall(SYS_wait, &exit_state);
        if (exit_state == 0)
            syscall(SYS_write, 1, sizeof(str_success), str_success);
        else
            syscall(SYS_write, 1, sizeof(str_fail), str_fail);
    }

    while(1);

    return 0;
}
