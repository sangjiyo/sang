/*
	测试三 普通文件与目录文件的操作 (2)
	1. mkdir chdir print_cwd
	2. link unlink
*/
#include "help.h"

void main(int argc, char *argv[])
{
	dentry_t de[10];
	file_stat_t stat;
	char tmp[32];
	uint32 root_fd, read_len, fd1, fd2;

	sys_print_cwd();
	
	sys_mkdir("new_workdir");
	sys_chdir("../.././new_workdir");
	sys_print_cwd();

	sys_mkdir("./2025_12_22");
	sys_mkdir("2025_12_22/19:00");
	sys_chdir("./2025_12_22/19:00");
	sys_print_cwd();

	fd1 = sys_open("./hello.txt", OPEN_READ | OPEN_WRITE | OPEN_CREATE);
	if (fd1 == -1) {
		fprintf(STDERR, "create hello.txt fail\n");
		sys_exit(1);
	}

	if (sys_link("./hello.txt", "/link.txt") == -1) {
		fprintf(STDERR, "link fail\n");
	}

	fd2 = sys_open("../../../link.txt", OPEN_READ | OPEN_WRITE);
	if (fd2 == -1) {
		fprintf(STDERR, "open link.txt fail\n");
		sys_exit(1);
	}

	fprintf(fd2, "hello world!");
	sys_read(fd1, 32, tmp);
	fprintf(STDOUT, "read data = %s\n", tmp);

	sys_fstat(fd1, &stat);
	print_fstat(&stat, "hello.txt");	

	sys_close(fd1);
	sys_close(fd2);

	sys_chdir("../../..");
	sys_print_cwd();

	root_fd = sys_open("/", OPEN_READ);
	read_len = sys_get_dentries(root_fd, de, 10 * sizeof(dentry_t));
	if (read_len == -1) {
		fprintf(STDERR, "sys_get_dentries fail 1\n");
		sys_exit(1);
	}
	print_dentries(de, read_len, "root");

	sys_unlink("./link.txt");
	sys_unlink("./new_workdir/2025_12_22/19:00/hello.txt");
	sys_unlink("./new_workdir/2025_12_22/19:00");
	sys_unlink("./new_workdir/2025_12_22");
	sys_unlink("./new_workdir");

	read_len = sys_get_dentries(root_fd, de, 10 * sizeof(dentry_t));
	if (read_len == -1) {
		fprintf(STDERR, "sys_get_dentries fail 2\n");
		sys_exit(1);
	}
	print_dentries(de, read_len, "root");

	sys_close(root_fd);
	sys_exit(0);
}