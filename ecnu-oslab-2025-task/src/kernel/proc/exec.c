#include "mod.h"

/*
    将ELF文件中的segment放入内存中制定位置
    inode逻辑区域: [seg_start, seg_start + len)
    进程地址空间: [va_start, va_start + len), 对应的物理页是存在的
*/
static void load_segment(inode_t* ip, pgtbl_t pgtbl,
    uint64 seg_start, uint64 va_start, uint32 len)
{
    assert(va_start % PGSIZE == 0, "load_segment: va aligned!");

    pte_t* pte;
    uint64 pa;
    uint32 read_len, cut_len;

    for (read_len = 0; read_len < len; read_len += PGSIZE)
    {
        pte = vm_getpte(pgtbl, va_start + read_len, false);
        pa = PTE_TO_PA(*pte);
        assert(pa != 0, "load_segment: invalid pa!");

        cut_len = MIN(len - read_len, PGSIZE);
        if (inode_read_data(ip, (uint32)seg_start + read_len, cut_len, (void*)pa, false) != cut_len)
            panic("load_segment: read fail!");
    }
}

/* 将程序的代码区和数据区读入用户堆中, 返回new_heap_top */
static uint64 prepare_heap(pgtbl_t new_pgtbl, inode_t* ip, elf_header_t* eh)
{
    program_header_t ph;
    uint64 new_heap_top = USER_BASE, old_heap_top = USER_BASE;

    for (uint32 off = eh->ph_off; off < eh->ph_off + eh->ph_ent_num * sizeof(ph); off += sizeof(ph))
    {
        if (inode_read_data(ip, off, sizeof(ph), &ph, false) != sizeof(ph))
            return (uint64)-1;

        if (ph.type != ELF_PROG_LOAD)
            continue;

        if (ph.mem_size < ph.file_size)
            return (uint64)-1;
        if (ph.va + ph.mem_size < ph.va)
            return (uint64)-1;
        if (ph.va % PGSIZE != 0)
            return (uint64)-1;

        int flag = PTE_R;
        if (ph.flags & ELF_PROG_FLAG_WRITE) flag |= PTE_W;
        if (ph.flags & ELF_PROG_FLAG_EXEC) flag |= PTE_X;

        new_heap_top = uvm_heap_grow(new_pgtbl, old_heap_top,
            ph.va + ph.mem_size - old_heap_top, flag);
        if (new_heap_top != ph.va + ph.mem_size)
            return (uint64)-1;
        old_heap_top = new_heap_top;

        load_segment(ip, new_pgtbl, ph.off, ph.va, ph.file_size);
    }

    return new_heap_top;
}

/* 准备栈空间用于存储输入参数, 设置arg_count, 返回sp */
static uint64 prepare_stack(pgtbl_t new_pgtbl, char** argv, int* arg_count)
{
    uint64 sp = USER_BASE + 64 * PGSIZE + PGSIZE;
    uint64 sp_base = USER_BASE + 64 * PGSIZE;
    uint64 sp_list[ELF_MAXARGS + 1];
    uint32 argc, arg_len;

    // 分配用户栈第一页
    uint64 ustack_page = (uint64)pmem_alloc(false);
    vm_mappages(new_pgtbl, sp_base, ustack_page, PGSIZE, PTE_R | PTE_W | PTE_U);

    for (argc = 0; argv[argc] != NULL; argc++)
    {
        if (argc >= ELF_MAXARGS)
            return (uint64)-1;

        arg_len = strlen(argv[argc]) + 1;
        sp -= ALIGN_UP(arg_len, 16);
        if (sp < sp_base)
            return (uint64)-1;

        uvm_copyout(new_pgtbl, sp, (uint64)argv[argc], arg_len);
        sp_list[argc] = sp;
    }
    sp_list[argc] = 0;

    arg_len = (argc + 1) * sizeof(uint64);
    sp -= ALIGN_UP(arg_len, 16);
    if (sp < sp_base)
        return (uint64)-1;

    uvm_copyout(new_pgtbl, sp, (uint64)sp_list, arg_len);

    *arg_count = argc;
    return sp;
}

int proc_exec(char* path, char** argv)
{
    proc_t* p = myproc();
    elf_header_t eh;
    inode_t* ip;
    pgtbl_t new_pgtbl;
    trapframe_t* new_tf;
    uint64 new_heap_top, sp;
    int argc;

    // Step 1: 打开ELF文件
    ip = path_to_inode(path);
    if (ip == NULL)
        return -1;
    inode_lock(ip);

    // Step 2: 验证ELF头
    if (inode_read_data(ip, 0, sizeof(eh), &eh, false) != sizeof(eh))
        goto bad;
    if (eh.magic != ELF_MAGIC)
        goto bad;

    // Step 3: 创建新的页表 (含trampoline + trapframe)
    new_tf = (trapframe_t*)pmem_alloc(false);
    memset(new_tf, 0, sizeof(trapframe_t));
    new_pgtbl = proc_pgtbl_init((uint64)new_tf);

    // Step 4: 加载ELF段到用户堆
    new_heap_top = prepare_heap(new_pgtbl, ip, &eh);
    if (new_heap_top == (uint64)-1)
        goto bad;

    // Step 5: 释放ELF inode
    inode_unlock(ip);
    inode_put(ip);
    ip = NULL;

    // Step 6: 准备参数栈
    sp = prepare_stack(new_pgtbl, argv, &argc);
    if (sp == (uint64)-1)
        goto bad;

    // Step 7: 切换到新的地址空间
    pgtbl_t old_pgtbl = p->pgtbl;
    p->pgtbl = new_pgtbl;
    p->tf = new_tf;
    p->heap_top = new_heap_top;
    p->ustack_npage = 1;
    p->mmap = NULL;

    // Step 8: 释放旧资源 (trapframe由uvm_destroy_pgtbl内部释放)
    if (old_pgtbl != NULL)
        uvm_destroy_pgtbl(old_pgtbl);

    // Step 9: 设置trapframe
    p->tf->user_to_kern_epc = eh.entry;
    p->tf->sp = sp;
    p->tf->a1 = sp;

    // Step 10: 更新进程名
    char* last = path;
    for (char* s = path; *s; s++)
        if (*s == '/') last = s + 1;
    memmove(p->name, last, MIN(strlen(last), 15));
    p->name[15] = '\0';

    // 验证: 代码页和trapframe不能共享同一物理页
    {
        pte_t* pte_code = vm_getpte(p->pgtbl, USER_BASE, false);
        pte_t* pte_tf = vm_getpte(p->pgtbl, TRAPFRAME, false);
        if (pte_code && pte_tf && (*pte_code & PTE_V) && (*pte_tf & PTE_V)) {
            uint64 pa_code = PTE_TO_PA(*pte_code);
            uint64 pa_tf = PTE_TO_PA(*pte_tf);
            if (pa_code == pa_tf)
                panic("exec: CODE and TRAPFRAME share same PA!");
        }
    }

    return argc;

bad:
    if (ip) {
        inode_unlock(ip);
        inode_put(ip);
    }
    return -1;
}
