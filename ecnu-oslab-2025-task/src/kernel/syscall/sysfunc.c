#include "mod.h"
extern int alloc_cnt; // 在 mmap.c 中声明

/*
    测试: 从用户空间传入一个int类型的数组
    uint64 addr 数组起始地址
    uint32 len  元素数量
    成功返回0
*/
uint64 sys_copyin()
{
    uint64 addr;
    uint32 len;
    arg_uint64(0, &addr);
    arg_uint32(1, &len);
    // 分配临时缓冲区（动态分配或使用栈，这里简单限制长度）
    if (len > 256)
        panic("sys_copyin: len too large");
    int buf[256];
    uvm_copyin(myproc()->pgtbl, (uint64)buf, addr, len * sizeof(int));
    printf("Kernel received: ");
    for (int i = 0; i < len; i++)
        printf("%d ", buf[i]);
    printf("\n");
    return 0;  // 成功返回0
}

/*
    测试: 向用户空间传出一个int类型的数组
    uint64 addr 数组起始地址
    成功返回拷贝的元素数量
*/
uint64 sys_copyout()
{
    uint64 addr;
    arg_uint64(0, &addr);
    int data[] = { 1, 2, 3, 4, 5 };
    uvm_copyout(myproc()->pgtbl, addr, (uint64)data, sizeof(data));
    return sizeof(data) / sizeof(int);  // 返回拷贝的元素数量 (5)
}

/*
    测试: 从用户空间传入一个字符串
    uint64 addr 字符串起始地址
    成功返回0
*/
uint64 sys_copyinstr()
{
    uint64 addr;
    arg_uint64(0, &addr);
    printf("sys_copyinstr: addr = %p\n", addr);
    char buf[128];
    uvm_copyin_str(myproc()->pgtbl, (uint64)buf, addr, sizeof(buf));
    printf("get string for user: %s\n", buf);
    return 0;  // 成功返回0
}

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