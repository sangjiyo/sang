#include "help.h"
#include "sys.h"

/*
    用户堆空间伸缩
    成功返回new_heap_top, 失败返回-1
*/
uint64 sys_brk(uint64 new_heap_top)
{
	return syscall(SYS_brk, new_heap_top); 
}

/*
    增加一段内存映射
    成功返回映射空间的起始地址, 失败返回-1
*/
uint64 sys_mmap(uint64 start, uint32 len)
{
	return syscall(SYS_mmap, start, len);
}

/*
    解除一段内存映射
    成功返回0 失败返回-1
*/
uint64 sys_munmap(uint64 start, uint32 len)
{
	return syscall(SYS_munmap, start, len);
}

/*
    进程复制
    返回子进程的pid
*/
uint32 sys_fork()
{
	return syscall(SYS_fork);
}

/*
    等待子进程退出
    成功返回子进程的pid, 失败返回-1
*/
uint32 sys_wait(uint32 *exit_state)
{
	return syscall(SYS_wait, exit_state);	
}

/*
	进程退出
	不返回
*/
void sys_exit(uint32 exit_state)
{
	syscall(SYS_exit, exit_state);
}

/*
	让进程睡眠一段时间(每个tick大约0.1秒)
	成功返回0
*/
uint32 sys_sleep(uint32 ntick)
{
	return syscall(SYS_sleep, ntick);
}

/*
	返回当前进程的pid
*/
uint32 sys_getpid()
{
	return syscall(SYS_getpid);
}

/*
    执行ELF文件以替换当前进程的内容
    成功返回argc, 失败返回-1	
*/
uint32 sys_exec(char *path, char **argv)
{
	return syscall(SYS_exec, path, argv);
}

/*
	打开或创建文件
	成功返回fd, 失败返回-1
*/
uint32 sys_open(char *path, uint32 open_mode)
{
	return syscall(SYS_open, path, open_mode);
}

/*
	关闭文件
	成功返回0, 失败返回-1
*/
uint32 sys_close(uint32 fd)
{
	return syscall(SYS_close, fd);
}

/*
	读取文件内容
	成功返回读到的字节数, 失败返回0
*/
uint32 sys_read(uint32 fd, uint32 len, void *addr)
{
	return syscall(SYS_read, fd, len, addr);
}

/*
	写入文件内容
	成功返回写入的字节数, 失败返回0
*/
uint32 sys_write(uint32 fd, uint32 len, void *addr)
{
	return syscall(SYS_write, fd, len, addr);
}

/*
	调整读写指针的位置
	成功返回新的偏移量, 失败返回-1
*/
uint32 sys_lseek(uint32 fd, uint32 offset, uint32 flag)
{
	return syscall(SYS_lseek, fd, offset, flag);
}

/*
    复制文件控制权
    成功返回new_fd, 失败返回-1
*/
uint32 sys_dup(uint32 fd)
{
	return syscall(SYS_dup, fd);
}

/*
	获取文件信息
	成功返回0, 失败返回-1
*/
uint32 sys_fstat(uint32 fd, file_stat_t *stat)
{
	return syscall(SYS_fstat, fd, stat);
}

/*
	获取目录中的所有目录项信息
	成功返回读到的字节数, 失败返回-1
*/
uint32 sys_get_dentries(uint32 fd, dentry_t *buf, uint32 buf_len)
{
	return syscall(SYS_get_dentries, fd, buf, buf_len);
}

/*
	创建一个目录
	成功返回0, 失败返回-1
*/
uint32 sys_mkdir(char *path)
{
	return syscall(SYS_mkdir, path);
}

/*
	修改当前工作目录
	成功返回0, 失败返回-1
*/
uint32 sys_chdir(char *new_path)
{
	return syscall(SYS_chdir, new_path);
}

/*
	打印当前工作目录的绝对路径
*/
uint32 sys_print_cwd()
{
	return syscall(SYS_print_cwd);
}

/*
	新建硬链接
	成功返回0, 失败返回-1
*/
uint32 sys_link(char *old_path, char *new_path)
{
	return syscall(SYS_link, old_path, new_path);
}

/*
	解除硬链接
	成功返回0, 失败返回-1
*/
uint32 sys_unlink(char *path)
{
	return syscall(SYS_unlink, path);
}