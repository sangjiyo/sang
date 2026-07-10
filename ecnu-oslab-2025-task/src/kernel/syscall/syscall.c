#include "mod.h"

// 跳转表: 系统调用号 -> 系统调用服务函数
static uint64 (*syscalls[])(void) = {
    [SYS_brk] sys_brk,
    [SYS_mmap] sys_mmap,
    [SYS_munmap] sys_munmap,
    [SYS_print_str] sys_print_str,
    [SYS_print_int] sys_print_int,
    [SYS_getpid] sys_getpid,
    [SYS_fork] sys_fork,
    [SYS_wait] sys_wait,
    [SYS_exit] sys_exit,
    [SYS_sleep] sys_sleep,
};

// 基于系统调用表的请求跳转
void syscall()
{
    proc_t *p = myproc();

    int sys_num = p->tf->a7;
    if (sys_num < 0 || sys_num > SYS_MAX_NUM || syscalls[sys_num] == NULL) {
        printf("unknown syscall %d from pid = %d\n", sys_num, p->pid);
        panic("syscall");
    } else {
        p->tf->a0 = syscalls[sys_num]();
    }
}

/*
    其他用于读取传入参数的函数
    参数分为两种,第一种是数据本身,第二种是指针
    第一种使用tf->ax传递
    第二种使用uvm_copyin 和 uvm_copyinstr 进行传递
*/

// 读取 n 号参数,它放在 an 寄存器中
static uint64 arg_raw(int n)
{
    proc_t *proc = myproc();
    
    switch (n)
    {
    case 0:
        return proc->tf->a0;
    case 1:
        return proc->tf->a1;
    case 2:
        return proc->tf->a2;
    case 3:
        return proc->tf->a3;
    case 4:
        return proc->tf->a4;
    case 5:
        return proc->tf->a5;
    default:
        panic("arg_raw: illegal arg num");
        return -1;
    }
}

// 读取 n 号参数, 作为 uint32 存储
void arg_uint32(int n, uint32 *ip)
{
    *ip = arg_raw(n);
}

// 读取 n 号参数, 作为 uint64 存储
void arg_uint64(int n, uint64 *ip)
{
    *ip = arg_raw(n);
}

// 读取 n 号参数指向的字符串到 buf, 字符串最大长度是 maxlen
void arg_str(int n, char *buf, int maxlen)
{
    proc_t *p = myproc();
    uint64 addr;
    arg_uint64(n, &addr);

    uvm_copyin_str(p->pgtbl, (uint64)buf, addr, maxlen);
}
