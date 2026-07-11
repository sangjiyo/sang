/*
    shell: 命令行解释器
    支持内置命令: cd, pwd, exit, echo, clear, help
    支持外部程序: fork + exec
*/
#include "help.h"

#define MAX_INPUT 256
#define MAX_ARGS   16
#define MAX_PATH   128

void main(int argc, char *argv[])
{
    char input[MAX_INPUT];
    char *args[MAX_ARGS + 1];
    char path[MAX_PATH];
    int arg_count;

    fprintf(STDOUT, "\nWelcome to ECNU-OS Shell!\n");
    fprintf(STDOUT, "Type 'help' for available commands.\n\n");

    while (1) {
        /* 显示提示符 */
        fprintf(STDOUT, "$ ");

        /* 读取一行输入 */
        int n = stdin(input, MAX_INPUT);
        if (n <= 0) continue;

        /* 去掉末尾换行符 */
        if (n > 0 && input[n - 1] == '\n')
            input[--n] = '\0';
        if (input[0] == '\0') continue;

        /* 解析参数 (按空格分割) */
        arg_count = 0;
        int in_token = 0;
        for (int i = 0; ; i++) {
            char c = input[i];
            if (c == ' ' || c == '\0') {
                if (in_token) {
                    input[i] = '\0';
                    in_token = 0;
                }
            } else if (!in_token) {
                if (arg_count < MAX_ARGS)
                    args[arg_count++] = &input[i];
                in_token = 1;
            }
            if (c == '\0') break;
        }
        args[arg_count] = NULL;
        if (arg_count == 0) continue;

        char *cmd = args[0];

        /* ---- 内置命令 ---- */

        /* cd <dir> */
        if (strncmp(cmd, "cd", 3) == 0 && (cmd[3] == '\0' || cmd[3] == ' ')) {
            char *dir = args[1];
            if (dir == NULL) dir = "/";
            if (sys_chdir(dir) == (uint32)-1)
                fprintf(STDERR, "cd: no such directory\n");
            continue;
        }

        /* pwd */
        if (strncmp(cmd, "pwd", 4) == 0) {
            sys_print_cwd();
            continue;
        }

        /* exit */
        if (strncmp(cmd, "exit", 5) == 0) {
            fprintf(STDOUT, "Goodbye!\n");
            sys_exit(0);
        }

        /* echo <args...> */
        if (strncmp(cmd, "echo", 5) == 0) {
            for (int i = 1; i < arg_count; i++) {
                if (i > 1) fprintf(STDOUT, " ");
                fprintf(STDOUT, "%s", args[i]);
            }
            fprintf(STDOUT, "\n");
            continue;
        }

        /* clear */
        if (strncmp(cmd, "clear", 6) == 0) {
            fprintf(STDOUT, "\033[2J\033[H");
            continue;
        }

        /* help */
        if (strncmp(cmd, "help", 5) == 0) {
            fprintf(STDOUT, "Built-in commands:\n");
            fprintf(STDOUT, "  cd <dir>    change directory\n");
            fprintf(STDOUT, "  pwd         print working directory\n");
            fprintf(STDOUT, "  echo <...>  print text\n");
            fprintf(STDOUT, "  clear       clear screen\n");
            fprintf(STDOUT, "  exit        exit shell\n");
            fprintf(STDOUT, "  help        show this help\n");
            fprintf(STDOUT, "\nExternal programs:\n");
            fprintf(STDOUT, "  ls          list directory\n");
            fprintf(STDOUT, "  cat  <file> print file\n");
            fprintf(STDOUT, "  time <cmd>  time a command\n");
            fprintf(STDOUT, "  reboot      reboot system\n");
            continue;
        }

        /* ---- 外部命令: fork + exec ---- */

        int pid = sys_fork();
        if (pid < 0) {
            fprintf(STDERR, "shell: fork failed\n");
        } else if (pid == 0) {
            /* 子进程: 构造程序绝对路径 "/<cmd>" */
            path[0] = '/';
            int j;
            for (j = 0; cmd[j] && j < MAXLEN_FILENAME - 2; j++)
                path[1 + j] = cmd[j];
            path[1 + j] = '\0';

            /* 构造子进程的 argv */
            char argv0[MAXLEN_FILENAME];
            for (j = 0; cmd[j] && j < MAXLEN_FILENAME - 1; j++)
                argv0[j] = cmd[j];
            argv0[j] = '\0';
            char *child_argv[MAX_ARGS + 1];
            child_argv[0] = argv0;
            for (int i = 1; i < arg_count; i++)
                child_argv[i] = args[i];
            child_argv[arg_count] = NULL;

            int ret = sys_exec(path, child_argv);
            if (ret == -1) {
                fprintf(STDERR, "shell: command not found: %s\n", cmd);
            }
            sys_exit(1);
        } else {
            /* 父进程: 等待子进程结束 */
            uint32 exit_state;
            sys_wait(&exit_state);
        }
    }
}
