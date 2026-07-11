/*
	测试二 普通文件与目录文件的操作 (1)
	1. open close dup fstat
	2. read write lseek get_dentries
*/

#include "help.h"

void main(int argc, char *argv[])
{
	file_stat_t stat;
	dentry_t *de;
	uint32 root_fd, root_fd_copy, ABC_fd;
	uint32 read_len = 0;
	char *tmp = (char*)sys_mmap(MMAP_BEGIN, PGSIZE);
	char ABC_str[] = "ABCDEFGHIJKLMNOPQRST";

	/* 1. 测试 opne close dup fstat*/

	root_fd = sys_open("/", OPEN_READ);
	if (root_fd == -1) {
		fprintf(STDERR, "open rootdir fail\n");
		sys_exit(1);
	}
	root_fd_copy = sys_dup(root_fd);

	sys_fstat(root_fd_copy, &stat);
	print_fstat(&stat, "root");
	sys_close(root_fd_copy);

	/* 2. 测试 read write lseek get_dentries */

	ABC_fd = sys_open("/ABC.txt", OPEN_READ | OPEN_WRITE | OPEN_CREATE);
	if (ABC_fd == -1) {
		fprintf(STDERR, "create ABC.txt fail\n");
		sys_exit(1);
	}

	for (int i = 0; i < 500; i++)
		fprintf(ABC_fd, "%d:%s ", i, ABC_str);

	sys_fstat(ABC_fd, &stat);
	print_fstat(&stat, "ABC.txt");

	sys_lseek(ABC_fd, 50, LSEEK_SUB);
	if (sys_read(ABC_fd, 50, tmp) != 50) {
		fprintf(STDERR, "read ABC.txt fail\n");
		sys_exit(1);
	}

	tmp[50] = '\0';
	fprintf(STDOUT, "read data = %s\n", tmp);

	de = (dentry_t*)tmp;
 	read_len = sys_get_dentries(root_fd, de, PGSIZE);
	if (read_len == -1) {
		fprintf(STDERR, "get dentries fail\n");
		sys_exit(1);
	}
	print_dentries(de, read_len, "root");

	sys_close(ABC_fd);
	sys_close(root_fd);
	sys_munmap((uint64)tmp, PGSIZE);
	sys_exit(0);
}