#include "mod.h"

/*
    用户堆空间伸缩
    uint64 new_heap_top (如果是0, 代表查询当前堆顶位置)
    成功返回new_heap_top, 失败返回-1
*/
uint64 sys_brk()
{
    uint64 new_heap_top;
    arg_uint64(0, &new_heap_top);

    proc_t* p = myproc();
    uint64 old_heap_top = p->heap_top;

    // 查询当前堆顶
    if (new_heap_top == 0) {
        printf("look event: ret_heap_top = %x\n", old_heap_top);
        vm_print(p->pgtbl);
        return old_heap_top;
    }

    // 边界检查：堆不能超过 MMAP_BEGIN
    if (new_heap_top > MMAP_BEGIN) {
        return -1;
    }

    // 增长
    if (new_heap_top > old_heap_top) {
        uint32 inc = new_heap_top - old_heap_top;
        uint64 ret = uvm_heap_grow(p->pgtbl, old_heap_top, inc);
        p->heap_top = ret;
        printf("grow event: ret_heap_top = %x\n", ret);
        vm_print(p->pgtbl);
        return ret;
    }
    // 收缩
    if (new_heap_top < old_heap_top) {
        uint32 dec = old_heap_top - new_heap_top;
        uint64 ret = uvm_heap_ungrow(p->pgtbl, old_heap_top, dec);
        p->heap_top = ret;
        printf("ungrow event: ret_heap_top = %x\n", ret);
        vm_print(p->pgtbl);
        return ret;
    }

    // 相等
    printf("equal event: ret_heap_top = %x\n", old_heap_top);
    vm_print(p->pgtbl);
    return old_heap_top;
}

/*
    增加一段内存映射
    uint64 start 起始地址
    uint32 len   范围 (字节,需检查是否是page-aligned)
    成功返回映射空间的起始地址, 失败返回-1
*/
uint64 sys_mmap()
{
    uint64 begin;
    uint32 len;
    arg_uint64(0, &begin);
    arg_uint32(1, &len);
    uint32 npages = (len + PGSIZE - 1) / PGSIZE;
    if (len == 0) return -1;
    // 若 begin 不为0，需检查页对齐
    if (begin != 0 && (begin % PGSIZE) != 0) {
        return -1;  // 地址未页对齐
    }
    // 调用 uvm_mmap，内部会检查范围并自动分配/映射
    uvm_mmap(begin, npages, PTE_R | PTE_W);
    proc_t* p = myproc();
    uvm_show_mmaplist(p->mmap);
    vm_print(p->pgtbl);
    printf("\n");
    mmap_region_t* last = p->mmap;
    while (last && last->next) last = last->next;
    if (begin == 0 && last) {
        return last->begin;
    }
    return begin;  // 如果begin非0，返回begin
}

/*
    解除一段内存映射
    uint64 start 起始地址
    uint32 len   范围 (字节, 需检查是否是page-aligned)
    成功返回0 失败返回-1
*/
uint64 sys_munmap()
{
    uint64 begin;
    uint32 len;
    arg_uint64(0, &begin);
    arg_uint32(1, &len);
    uint32 npages = (len + PGSIZE - 1) / PGSIZE;
    if (len == 0 || (begin % PGSIZE) != 0) {
        return -1;
    }
    uvm_munmap(begin, npages);
    proc_t* p = myproc();
    uvm_show_mmaplist(p->mmap);
    vm_print(p->pgtbl);
    printf("\n");
    return 0;
}

/*
    打印一个字符串
    char *str
    成功返回0
*/
uint64 sys_print_str()
{

}

/*
    打印一个32位整数
    int num
    成功返回0
*/
uint64 sys_print_int()
{

}

/*
    进程复制
    返回子进程的pid
*/
uint64 sys_fork()
{

}

/*
    等待子进程退出
    uint64 addr_exit_state
*/
uint64 sys_wait()
{

}

/*
    进程退出
    int exit_code
    不返回
*/
uint64 sys_exit()
{

}

/*
    让进程睡眠一段时间
    uint32 ntick (1个tick大约0.1秒)
    成功返回0
*/
uint64 sys_sleep()
{

}

/*
    返回当前进程的pid
*/
uint64 sys_getpid()
{

}