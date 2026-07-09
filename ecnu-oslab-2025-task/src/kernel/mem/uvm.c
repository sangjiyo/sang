#include "mod.h"

/*--------------------part-1: 关于内核空间<->用户空间的数据传递--------------------*/

// 用户态地址空间[src, src+len) 拷贝至 内核态地址空间[dst, dst+len)
// 注意: src dst 不一定是 page-aligned
void uvm_copyin(pgtbl_t pgtbl, uint64 dst, uint64 src, uint32 len)
{
    uint64 n, va0, pa0;
    while (len > 0) {
        va0 = ALIGN_DOWN(src, PGSIZE);
        pte_t* pte = vm_getpte(pgtbl, va0, 0);
        if(pte == NULL || !(*pte & PTE_V))
            panic("uvm_copyin: page fault");
        pa0 = PTE_TO_PA(*pte);
        n = PGSIZE - (src - va0);
        if (n > len) 
            n = len;
        memmove((void*)dst, (void*)(pa0 + (src - va0)), n);
        len -= n;
        src += n;
        dst += n;
    }
}

// 内核态地址空间[src, src+len） 拷贝至 用户态地址空间[dst, dst+len)
// 注意: src dst 不一定是 page-aligned
void uvm_copyout(pgtbl_t pgtbl, uint64 dst, uint64 src, uint32 len)
{
    uint64 n, va0, pa0;
    while (len > 0) {
        va0 = ALIGN_DOWN(dst, PGSIZE);
        pte_t* pte = vm_getpte(pgtbl, va0, 0);
        if (pte == NULL || !(*pte & PTE_V))
            panic("uvm_copyout: page fault");
        pa0 = PTE_TO_PA(*pte);
        n = PGSIZE - (dst - va0);
        if (n > len)
            n = len;
        memmove((void*)(pa0 + (dst - va0)), (void*)src, n);
        len -= n;
        dst += n;
        src += n;
    }
}

// 用户态字符串拷贝到内核态
// 最多拷贝maxlen字节, 中途遇到'\0'则终止
// 注意: src dst 不一定是 page-aligned
void uvm_copyin_str(pgtbl_t pgtbl, uint64 dst, uint64 src, uint32 maxlen)
{
    uint64 n, va0, pa0;
    int got_null = 0;
    char* kaddr = (char*)dst;

    while (got_null == 0 && maxlen > 0) {
        va0 = ALIGN_DOWN(src, PGSIZE);
        pte_t* pte = vm_getpte(pgtbl, va0, 0);
        if (pte == NULL || !(*pte & PTE_V))
            panic("uvm_copyin_str: page fault");
        pa0 = PTE_TO_PA(*pte);
        n = PGSIZE - (src - va0);
        if (n > maxlen) n = maxlen;

        char* p = (char*)(pa0 + (src - va0));
        while (n > 0) {
            if (*p == '\0') {
                *kaddr = '\0';
                got_null = 1;
                break;
            }
            else {
                *kaddr = *p;
            }
            --n;
            --maxlen;
            p++;
            kaddr++;
            src++;
        }
        // 跳到下一页（如果还没结束）
        if (!got_null)
            src = va0 + PGSIZE;
        else
            break;
    }
    if (!got_null)
        panic("uvm_copyin_str: string too long");
}

/*--------------------part-2: mmap_region相关--------------------*/

// 打印以mmap为首的mmap链
// for debug
void uvm_show_mmaplist(mmap_region_t *mmap)
{
    mmap_region_t *tmp = mmap;
    printf("\nalloced mmap_space:\n");
    if (tmp == NULL)
        printf("empty\n");
    while (tmp != NULL)
    {
        printf("alloced mmap_region: %x ~ %x\n", tmp->begin, tmp->begin + tmp->npages * PGSIZE);
        tmp = tmp->next;
    }
}

// 两个 mmap_region 区域合并
// 注意: 保留一个 释放一个 不操作 next 指针
// 由uvm_mmap调用
static void mmap_merge(mmap_region_t *mmap_1, mmap_region_t *mmap_2, bool keep_mmap_1)
{
    // 确保有效和紧临
    assert(mmap_1 != NULL && mmap_2 != NULL, "mmap_merge: NULL");
    assert(mmap_1->begin + mmap_1->npages * PGSIZE == mmap_2->begin, "mmap_merge: check fail");

    // merge
    if (keep_mmap_1) {
        mmap_1->npages += mmap_2->npages;
        mmap_region_free(mmap_2);
    } else {
        mmap_2->begin -= mmap_1->npages * PGSIZE;
        mmap_2->npages += mmap_1->npages;
        mmap_region_free(mmap_1);
    }
}

// 寻找一块足够大的区域(len), 作为 mmap_region
// 由uvm_mmap调用(处理begin==0的情况)
// 成功返回begin, 失败返回0
static uint64 uvm_mmap_find(mmap_region_t *head_mmap, uint64 len, mmap_region_t **p_last_mmap, mmap_region_t **p_tmp_mmap)
{
    // 遍历链表，寻找空闲空洞
    uint64 candidate = MMAP_BEGIN;
    mmap_region_t* prev = NULL, * cur = head_mmap;
    while (cur) {
        if (cur->begin > candidate && cur->begin - candidate >= len) {
            // 找到合适的空洞
            if (p_last_mmap) *p_last_mmap = prev;
            if (p_tmp_mmap) *p_tmp_mmap = cur;
            return candidate;
        }
        // 更新 candidate 为当前区域之后的地址
        candidate = cur->begin + cur->npages * PGSIZE;
        prev = cur;
        cur = cur->next;
    }
    // 检查末尾空闲区域
    if (MMAP_END - candidate >= len) {
        if (p_last_mmap) *p_last_mmap = prev;
        if (p_tmp_mmap) *p_tmp_mmap = NULL;
        return candidate;
    }
    return 0;
}

// 将 new_region 插入链表，保持按 begin 升序，并尝试合并相邻区域
static void mmap_insert_region(mmap_region_t** head, mmap_region_t* new_region)
{
    mmap_region_t* prev = NULL, * cur = *head;
    while (cur && cur->begin < new_region->begin) {
        prev = cur;
        cur = cur->next;
    }
    new_region->next = cur;
    if (prev) prev->next = new_region;
    else *head = new_region;

    // 与 prev 合并
    if (prev && prev->begin + prev->npages * PGSIZE == new_region->begin) {
        // 保留 prev，释放 new_region
        mmap_region_t* next = new_region->next;
        mmap_merge(prev, new_region, true);
        prev->next = next;
        new_region = prev; // 合并后区域为 prev
    }
    // 与 next 合并
    if (new_region->next && new_region->begin + new_region->npages * PGSIZE == new_region->next->begin) {
        mmap_region_t* next = new_region->next;
        mmap_region_t* next_next = next->next;
        mmap_merge(new_region, next, true);
        new_region->next = next_next;
    }
}

// 在用户页表和进程mmap链里新增mmap区域 [begin, begin + npages * PGSIZE)
// 调用者保证begin是page-aligned的, 页面权限为perm
// 注意: 如果start==0, 意味着需要内核自主找一块足够大的空间
// 失败则panic卡死
void uvm_mmap(uint64 begin, uint32 npages, int perm)
{
    proc_t* p = myproc();
    uint64 len = npages * PGSIZE;

    if (begin == 0) {
        // 自动分配：查找空闲空洞
        mmap_region_t* prev = NULL, * next = NULL;
        begin = uvm_mmap_find(p->mmap, len, &prev, &next);
        if (begin == 0)
            panic("uvm_mmap: no enough space");
        mmap_region_t* new_region = mmap_region_alloc();
        new_region->begin = begin;
        new_region->npages = npages;
        new_region->next = next;
        // 插入链表并合并
        mmap_insert_region(&p->mmap, new_region);
    }
    else {
        // 指定地址
        if (begin < MMAP_BEGIN || begin + len > MMAP_END)
            panic("uvm_mmap: out of mmap range");
        // 检查重叠
        mmap_region_t* cur = p->mmap;
        while (cur) {
            if (!(begin + len <= cur->begin || begin >= cur->begin + cur->npages * PGSIZE))
                panic("uvm_mmap: overlap");
            cur = cur->next;
        }
        mmap_region_t* new_region = mmap_region_alloc();
        new_region->begin = begin;
        new_region->npages = npages;
        new_region->next = NULL;
        mmap_insert_region(&p->mmap, new_region);
    }

    // 分配物理页并映射
    for (uint32 i = 0; i < npages; i++) {
        uint64 va = begin + i * PGSIZE;
        uint64 pa = (uint64)pmem_alloc(false);
        vm_mappages(p->pgtbl, va, pa, PGSIZE, perm | PTE_U);
    }
}


// 在用户页表和进程mmap链里释放mmap区域 [begin, begin + npages * PGSIZE)
// 失败则panic卡死
void uvm_munmap(uint64 begin, uint32 npages)
{
    proc_t* p = myproc();
    uint64 len = npages * PGSIZE;
    mmap_region_t* prev = NULL, * cur = p->mmap;

    // 找到包含 begin 的区域
    while (cur && cur->begin + cur->npages * PGSIZE <= begin) {
        prev = cur;
        cur = cur->next;
    }
    if (!cur || cur->begin > begin || cur->begin + cur->npages * PGSIZE < begin + len) {
        vm_print(p->pgtbl);
        panic("uvm_munmap: region not found");
    }

    // 情况1：完全重合
    if (cur->begin == begin && cur->npages == npages) {
        if (prev) prev->next = cur->next;
        else p->mmap = cur->next;
        // 解除映射并释放物理页
        for (uint32 i = 0; i < npages; i++) {
            uint64 va = begin + i * PGSIZE;
            pte_t* pte = vm_getpte(p->pgtbl, va, 0);
            if (!pte || !(*pte & PTE_V)) panic("uvm_munmap: invalid pte");
            uint64 pa = PTE_TO_PA(*pte);
            pmem_free(pa, false);
            *pte = 0;
        }
        mmap_region_free(cur);
        return;
    }

    // 情况2：解除头部
    if (cur->begin == begin) {
        cur->begin += len;
        cur->npages -= npages;
        for (uint32 i = 0; i < npages; i++) {
            uint64 va = begin + i * PGSIZE;
            pte_t* pte = vm_getpte(p->pgtbl, va, 0);
            if (!pte || !(*pte & PTE_V)) panic("uvm_munmap: invalid pte");
            uint64 pa = PTE_TO_PA(*pte);
            pmem_free(pa, false);
            *pte = 0;
        }
        return;
    }

    // 情况3：解除尾部
    if (cur->begin + cur->npages * PGSIZE == begin + len) {
        cur->npages -= npages;
        for (uint32 i = 0; i < npages; i++) {
            uint64 va = begin + i * PGSIZE;
            pte_t* pte = vm_getpte(p->pgtbl, va, 0);
            if (!pte || !(*pte & PTE_V)) panic("uvm_munmap: invalid pte");
            uint64 pa = PTE_TO_PA(*pte);
            pmem_free(pa, false);
            *pte = 0;
        }
        return;
    }

    // 情况4：解除中间，需要拆分
    mmap_region_t* new_region = mmap_region_alloc();
    new_region->begin = begin + len;
    new_region->npages = cur->npages - (begin - cur->begin) / PGSIZE - npages;
    cur->npages = (begin - cur->begin) / PGSIZE;
    new_region->next = cur->next;
    cur->next = new_region;

    for (uint32 i = 0; i < npages; i++) {
        uint64 va = begin + i * PGSIZE;
        pte_t* pte = vm_getpte(p->pgtbl, va, 0);
        if (!pte || !(*pte & PTE_V)) panic("uvm_munmap: invalid pte");
        uint64 pa = PTE_TO_PA(*pte);
        pmem_free(pa, false);
        *pte = 0;
    }
}

/*------------------part-3: 用户空间heap和stack管理相关------------------*/

// 用户堆空间增加, 返回新的堆顶地址 (注意栈顶最大值限制)
uint64 uvm_heap_grow(pgtbl_t pgtbl, uint64 cur_heap_top, uint32 len) 
{
    uint64 new_top = cur_heap_top + len;
    if (new_top > MMAP_BEGIN)
        panic("uvm_heap_grow: exceed mmap region");
    uint64 start = ALIGN_UP(cur_heap_top, PGSIZE);
    uint64 end = ALIGN_UP(new_top, PGSIZE);
    for (uint64 va = start; va < end; va += PGSIZE) {
        uint64 pa = (uint64)pmem_alloc(false);
        vm_mappages(pgtbl, va, pa, PGSIZE, PTE_R | PTE_W | PTE_U);
    }
    return new_top;
}

// 用户堆空间减少, 返回新的堆顶地址
uint64 uvm_heap_ungrow(pgtbl_t pgtbl, uint64 cur_heap_top, uint32 len)
{
    uint64 new_top = cur_heap_top - len;
    if (new_top < USER_BASE + PGSIZE)
        panic("uvm_heap_ungrow: too low");
    uint64 start = ALIGN_UP(new_top, PGSIZE);
    uint64 end = ALIGN_UP(cur_heap_top, PGSIZE);
    for (uint64 va = start; va < end; va += PGSIZE) {
        pte_t* pte = vm_getpte(pgtbl, va, 0);
        if (!pte || !(*pte & PTE_V))
            panic("uvm_heap_ungrow: invalid pte");
        uint64 pa = PTE_TO_PA(*pte);
        pmem_free(pa, false);
        *pte = 0;
    }
    return new_top;
}

// 处理函数栈增长导致的page fault事件
// 成功返回new_ustack_npage，失败返回-1
uint64 uvm_ustack_grow(pgtbl_t pgtbl, uint64 old_ustack_npage, uint64 fault_addr)
{
    // 初始栈低地址 (0x3000)
    uint64 ustack_va = USER_BASE + 64 * PGSIZE;
    uint64 current_low = ustack_va - (old_ustack_npage - 1) * PGSIZE;

    // 计算需要新增的页数
    uint64 need_pages = (current_low - fault_addr + PGSIZE - 1) / PGSIZE;
    if (need_pages == 0) need_pages = 1;
    // 限制不能超过堆区域
    uint64 max_pages = (current_low - (USER_BASE + PGSIZE)) / PGSIZE;
    if (need_pages > max_pages)
        panic("uvm_ustack_grow: not enough space for stack");
    // 确保至少扩展 4 页，避免后续多次缺页
    if (need_pages < 4) need_pages = 4;
    
    // 新栈低地址，必须 ≥ MMAP_END
    uint64 new_npage = old_ustack_npage + need_pages;
    uint64 new_low = ustack_va - (new_npage - 1) * PGSIZE;
    if (new_low < USER_BASE + PGSIZE)
        panic("uvm_ustack_grow: stack collides with heap");

    // 分配新物理页并映射到 current_low 以下的地址
    for (uint64 i = 0; i < need_pages; i++) {
        uint64 va = current_low - (i + 1) * PGSIZE;
        if (va < USER_BASE + PGSIZE) // 安全检查
            panic("uvm_ustack_grow: va below heap");
        uint64 pa = (uint64)pmem_alloc(false);
        vm_mappages(pgtbl, va, pa, PGSIZE, PTE_R | PTE_W | PTE_U);
    }

    return new_npage;
}

/*----------------------part-4: 用户页表管理相关----------------------*/

// 递归释放 页表占用的物理页 和 页表管理的物理页
// ps: 顶级页表level = 3
static void destroy_pgtbl(pgtbl_t pgtbl, uint32 level)
{
    for (int i = 0; i < 512; i++) {
        pte_t pte = pgtbl[i];
        if (pte & PTE_V) {
            if ((pte & (PTE_R | PTE_W | PTE_X)) == 0) {
                pgtbl_t next = (pgtbl_t)PTE_TO_PA(pte);
                destroy_pgtbl(next, level - 1);
                pmem_free((uint64)next, true);
            }
            else {
                uint64 pa = PTE_TO_PA(pte);
                if (pa >= (uint64)ALLOC_BEGIN && pa < (uint64)ALLOC_END) {
                    pmem_free(pa, false);
                }
            }
        }
    }
}

// 页表销毁
void uvm_destroy_pgtbl(pgtbl_t pgtbl)
{
    vm_unmappages(pgtbl, TRAPFRAME, PGSIZE, true);   // 可以释放，因为trapframe是每个进程独有的
    vm_unmappages(pgtbl, TRAMPOLINE, PGSIZE, false); // 不能释放，因为所有进程共用区域
    destroy_pgtbl(pgtbl, 3);
    pmem_free((uint64)pgtbl, true);
}

// 连续虚拟空间的复制
// 在uvm_copy_pgtbl中使用
static void copy_range(pgtbl_t old, pgtbl_t new, uint64 begin, uint64 end)
{
    uint64 va, pa, page;
    int flags;
    pte_t *pte;

    for (va = begin; va < end; va += PGSIZE)
    {
        pte = vm_getpte(old, va, false);
        assert(pte != NULL, "uvm_copy_pgtbl: pte == NULL");
        assert((*pte) & PTE_V, "uvm_copy_pgtbl: pte not valid");

        pa = (uint64)PTE_TO_PA(*pte);
        flags = (int)PTE_FLAGS(*pte);

        page = (uint64)pmem_alloc(false);
        memmove((char *)page, (const char *)pa, PGSIZE);
        vm_mappages(new, va, page, PGSIZE, flags);
    }
}

// 拷贝页表 (拷贝并不包括 trapframe 和 trampoline)
// 拷贝的页表管理的物理页是原来页表的复制品
void uvm_copy_pgtbl(pgtbl_t old, pgtbl_t new, uint64 heap_top, uint64 ustack_npage, mmap_region_t *mmap)
{
    copy_range(old, new, USER_BASE, USER_BASE + PGSIZE);
    copy_range(old, new, USER_BASE + PGSIZE, heap_top);
    uint64 stack_top = USER_BASE + 2 * PGSIZE + ustack_npage * PGSIZE;
    copy_range(old, new, USER_BASE + 2 * PGSIZE, stack_top);
    mmap_region_t* r = mmap;
    while (r) {
        copy_range(old, new, r->begin, r->begin + r->npages * PGSIZE);
        r = r->next;
    }
}
