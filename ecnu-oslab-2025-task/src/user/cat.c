/*
    cat: 显示文件内容
    用法: cat <file>
*/
#include "help.h"

void main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(STDERR, "usage: cat <file>\n");
        sys_exit(1);
    }

    uint32 fd = sys_open(argv[1], OPEN_READ);
    if (fd == (uint32)-1) {
        fprintf(STDERR, "cat: cannot open '%s'\n", argv[1]);
        sys_exit(1);
    }

    /* 分配读缓冲区 */
    uint32 buf_len = PGSIZE;
    char *buf = (char*)sys_mmap(MMAP_BEGIN, buf_len);
    if ((uint64)buf == (uint64)-1) {
        fprintf(STDERR, "cat: mmap failed\n");
        sys_close(fd);
        sys_exit(1);
    }

    /* 循环读取并输出 */
    for (;;) {
        int32 n = sys_read(fd, buf_len, buf);
        if (n <= 0) break;
        sys_write(STDOUT, n, buf);
    }

    sys_munmap((uint64)buf, buf_len);
    sys_close(fd);
    sys_exit(0);
}
