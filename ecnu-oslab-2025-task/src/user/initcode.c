#include "sys.h"

int main()
{
	char path[] = "./test_1";
	char arg0[] = "test_1";
	char arg1[] = "111";
	char arg2[] = "222";
	char arg3[] = "333";
	char *argv[] = {arg0, arg1, arg2, arg3, 0};
	
	char str_1[] = "initcode: fork fail!\n";
	char str_2[] = "\n======== test start  ========\n\n";
	char str_3[] = "\n======== test sucess ========\n";
	char str_4[] = "\n======== test fail   ========\n";
	char str_5[] = "initcode: exec fail!\n";

	int ret, pid = syscall(SYS_fork);

	if (pid < 0) {
		syscall(SYS_write, 1, sizeof(str_1), str_1);
	} else if (pid == 0) {
		syscall(SYS_write, 1, 4, "run ");
		syscall(SYS_write, 1, sizeof(path), path);
		for (int i = 0; argv[i] != 0; i++) {
			syscall(SYS_write, 1, 1, " ");
			syscall(SYS_write, 1, sizeof(argv[i]), argv[i]);
		}
		syscall(SYS_write, 1, 1, "\n");
		syscall(SYS_write, 1, sizeof(str_2), str_2);
		ret = (int)syscall(SYS_exec, path, argv);
		if (ret != 0) {
			syscall(SYS_write, 1, sizeof(str_5), str_5);
			syscall(SYS_exit, 1);
		}
	} else {
		unsigned int exit_state = 0;
		syscall(SYS_wait, &exit_state);
		if (exit_state == 0)
			syscall(SYS_write, 1, sizeof(str_3), str_3);
		else
			syscall(SYS_write, 1, sizeof(str_4), str_4);
	}

	while(1);

	return 0;
}