/*
    time: 测量命令执行时间
    用法: time <command> [args...]
    显示命令执行消耗的时钟滴答数和大约秒数
*/
#include "help.h"

#define MAX_ARGS 16
#define MAX_PATH 128

void main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(STDERR, "usage: time <command> [args...]\n");
        sys_exit(1);
    }

    /* 构造命令路径 "./<cmd>" */
    char path[MAX_PATH];
    path[0] = '/';
    int j;
    for (j = 0; argv[1][j] && j < MAX_PATH - 2; j++)
        path[1 + j] = argv[1][j];
    path[1 + j] = '\0';

    /* 记录开始时间 */
    uint64 start = sys_uptime();

    int pid = sys_fork();
    if (pid < 0) {
        fprintf(STDERR, "time: fork failed\n");
        sys_exit(1);
    } else if (pid == 0) {
        /* 子进程: 构造 argv 并执行 */
        char *child_argv[MAX_ARGS + 1];
        child_argv[0] = argv[1];
        for (int i = 2; i < argc && i - 2 < MAX_ARGS; i++)
            child_argv[i - 1] = argv[i];
        child_argv[argc - 1] = NULL;

        sys_exec(path, child_argv);
        fprintf(STDERR, "time: exec '%s' failed\n", argv[1]);
        sys_exit(1);
    } else {
        /* 父进程: 等待子进程结束 */
        uint32 exit_state;
        sys_wait(&exit_state);

        /* 记录结束时间 */
        uint64 end = sys_uptime();
        uint64 elapsed = end - start;

        /* 输出耗时 (约 0.1 秒/tick) */
        fprintf(STDOUT, "elapsed: %d ticks (about %d.%d seconds)\n",
            (uint32)elapsed,
            (uint32)(elapsed / 10),
            (uint32)(elapsed % 10));
    }

    sys_exit(0);
}
