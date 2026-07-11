#include "mod.h"

device_t device_table[N_DEVICE];

/* 标准输入设备 */
static uint32 device_stdin_read(uint32 len, uint64 dst, bool is_user_dst)
{
	return cons_read(len, dst, is_user_dst);
}

/* 标准输出设备 */
static uint32 device_stdout_write(uint32 len, uint64 src, bool is_user_src)
{
	return cons_write(len, src, is_user_src);
}

/* 标准错误输出设备 */
static uint32 device_stderr_write(uint32 len, uint64 src, bool is_user_src)
{
	printf("ERROR: ");
	return cons_write(len, src, is_user_src);
}

/* 无限0流 */
static uint32 device_zero_read(uint32 len, uint64 dst, bool is_user_dst)
{
	uint32 write_len = 0, cut_len = 0;

	uint64 src = (uint64)pmem_alloc(true);
	proc_t *p = myproc();

	while (write_len < len)
	{
		cut_len = MIN(len - write_len, PGSIZE);
		
		if (is_user_dst)
			uvm_copyout(p->pgtbl, dst, src, cut_len);
		else
			memmove((void*)dst, (void*)src, cut_len);

		dst += cut_len;
		write_len += cut_len;
	}

	pmem_free(src, true);

	return write_len;
}

/* 空设备读取 */
static uint32 device_null_read(uint32 len, uint64 dst, bool is_user_dst)
{
	return 0;
}

/* 空设备写入 */
static uint32 device_null_write(uint32 len, uint64 src, bool is_user_src)
{
	return len;
}

/* 彩蛋: 笨蛋GPT */
static uint32 device_gpt0_write(uint32 len, uint64 src, bool is_user_src)
{
	char tmp[STR_MAXLEN + 1];
	proc_t *p = myproc();

	tmp[len] = '\0';

	if (is_user_src)
		uvm_copyin(p->pgtbl, (uint64)tmp, src, len);
	else
		memmove(tmp, (void*)src, len);

	if (strncmp(tmp, "Hello", len) == 0) {
		printf("Hi, I am gpt0!\n");
	} else if (strncmp(tmp, "Guess who I am", len) == 0) {
		printf("Your procid is %d and name is %s.\n", p->pid, p->name);
	} else if (strncmp(tmp, "How many free memory left", len) == 0) {
		uint32 kernel_free_pages, user_free_pages;
		pmem_stat(&kernel_free_pages, &user_free_pages);
		printf("We have %d free pages in kernel space, %d free pages in user space!\n",
			kernel_free_pages, user_free_pages);
	} else if (strncmp(tmp, "Good job", len) == 0) {
		printf("Thanks for your kind words!\n");
	} else {
		printf("Sorry, I can not understand it.\n");
	}

	return len;
}

/* 注册设备 */
static void device_register(uint32 index, char* name,
	uint32(*read)(uint32, uint64, bool),
	uint32(*write)(uint32, uint64, bool))
{
	memmove(device_table[index].name, name, MAXLEN_FILENAME);
	device_table[index].read = read;
	device_table[index].write = write;
}

/* 初始化device_table */
void device_init()
{

}

/* 检查文件major字段的合法性 */
bool device_open_check(uint16 major, uint32 open_mode)
{

}

/* 从设备文件中读取数据 */
uint32 device_read_data(uint16 major, uint32 len, uint64 dst, bool is_user_dst)
{

}

/* 向设备文件写入数据 */
uint32 device_write_data(uint16 major, uint32 len, uint64 src, bool is_user_src)
{

}
