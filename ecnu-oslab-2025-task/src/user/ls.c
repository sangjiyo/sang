/*
    ls: 列出目录内容
    用法: ls [path]
    默认列出当前工作目录
*/
#include "help.h"

void main(int argc, char *argv[])
{
    char *dir_path = ".";
    if (argc > 1)
        dir_path = argv[1];

    uint32 fd = sys_open(dir_path, OPEN_READ);
    if (fd == (uint32)-1) {
        fprintf(STDERR, "ls: cannot open '%s'\n", dir_path);
        sys_exit(1);
    }

    /* 为目录项分配缓冲区 */
    uint32 buf_len = 16 * sizeof(dentry_t);
    char *buf = (char*)sys_mmap(MMAP_BEGIN, buf_len);
    if ((uint64)buf == (uint64)-1) {
        fprintf(STDERR, "ls: mmap failed\n");
        sys_close(fd);
        sys_exit(1);
    }

    dentry_t *de = (dentry_t*)buf;
    int32 read_len = sys_get_dentries(fd, de, buf_len);

    if (read_len < 0) {
        fprintf(STDERR, "ls: read dir failed\n");
    } else {
        uint32 count = read_len / sizeof(dentry_t);
        for (uint32 i = 0; i < count; i++) {
            fprintf(STDOUT, "%s\n", de[i].name);
        }
    }

    sys_munmap((uint64)buf, buf_len);
    sys_close(fd);
    sys_exit(0);
}
