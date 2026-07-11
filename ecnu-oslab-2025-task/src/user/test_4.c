/*
	测试四 设备文件的操作
	/dev/null /dev/zero /dev/gpt0
*/
#include "help.h"

void main(int argc, char *argv[])
{
	uint32 fd1, fd2;
	uint32 len, tmp[32];
	char str[MAXLEN_STR + 1];
	dentry_t de[10];

	fd1 = sys_open("dev", OPEN_READ);
	if (fd1 == -1) {
		fprintf(STDERR, "open dev fail\n");
		sys_exit(1);
	}

	len = sys_get_dentries(fd1, de, sizeof(dentry_t) * 10);
	if (len == -1) {
		fprintf(STDERR, "get dentries fail\n");
		sys_exit(1);
	}
	print_dentries(de, len, "dev");
	sys_close(fd1);
	
 	if (sys_chdir("dev") == -1) {
		fprintf(STDERR, "change dir fail\n");
		sys_exit(1);
	}

	fd2 = sys_open("zero", OPEN_READ);
	len = sys_read(fd2, sizeof(tmp), tmp);
	if (len != sizeof(tmp)) {
		fprintf(STDERR, "/dev/zero fail\n");
		sys_exit(1);
	}
	for (int i = 0; i < 32; i++)
		fprintf(STDOUT, "%d ", tmp[i]);
	fprintf(STDOUT, "\n", 1);
	sys_close(fd2);

	fd2 = sys_open("null", OPEN_READ | OPEN_WRITE);
    len = sys_write(fd2, sizeof(tmp), tmp);
	if (len != sizeof(tmp)) {
		fprintf(STDERR, "/dev/null write fail\n");
		sys_exit(1);
	}
	len = sys_read(fd2, sizeof(tmp), tmp);
	if (len != 0) {
		fprintf(STDERR, "/dev/null read fail\n");
		sys_exit(1);		
	}
	sys_close(fd2);

	fd2 = sys_open("gpt0", OPEN_WRITE);
	for (int i = 1; i <= 4; i++) {
		fprintf(STDOUT, "Q%d: ", i);
		len = stdin(str, MAXLEN_STR);
		fprintf(STDOUT, "A%d: ", i);
		sys_write(fd2, len - 1, str); // len-1是为了跳过'\n'
	}
	sys_close(fd2);

	sys_exit(0);
}