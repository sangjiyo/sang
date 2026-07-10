#include "mod.h"

// 内核页表
static pgtbl_t kernel_pgtbl;

// 根据pagetable,找到va对应的pte
// 若设置alloc=true 则在PTE无效时尝试申请一个物理页
// 成功返回PTE, 失败返回NULL
// 提示：使用 VA_TO_VPN + PTE_TO_PA + PA_TO_PTE
pte_t *vm_getpte(pgtbl_t pgtbl, uint64 va, bool alloc)
{
    if (va >= VA_MAX)
        panic("vm_getpte: va too large");

    // 如果传入NULL, 使用内核页表 (供virtio_disk_rw等内核代码使用)
    if (pgtbl == NULL)
        pgtbl = kernel_pgtbl;

    pgtbl_t cur = pgtbl;
    for (int level = 2; level > 0; level--) {
        int idx = VA_TO_VPN(va, level);
        pte_t* pte = &cur[idx];
        if (!(*pte & PTE_V)) {
            // 下级页表不存在
            if (!alloc)
                return NULL;
            // 分配一个物理页作为下级页表
            uint64 new_pgtbl_pa = (uint64)pmem_alloc(true); // 内核分配
            memset((void*)new_pgtbl_pa, 0, PGSIZE);
            // 设置当前页表项指向该物理页，权限为0（因为是页表）
            *pte = PA_TO_PTE(new_pgtbl_pa) | PTE_V;
        }
        // 跳到下级页表（注意：PTE中存放的是物理地址，但内核直接映射，所以可当作虚拟地址使用）
        cur = (pgtbl_t)PTE_TO_PA(*pte);
    }
    // level = 0
    int idx = VA_TO_VPN(va, 0);
    return &cur[idx];
}

// 在pgtbl中建立 [va, va + len) -> [pa, pa + len) 的映射
// 本质是找到va在页表对应位置的pte并修改它
// 检查: va pa 应当是 page-aligned, len(字节数) > 0, va + len <= VA_MAX
// 注意: perm 应该如何使用
void vm_mappages(pgtbl_t pgtbl, uint64 va, uint64 pa, uint64 len, int perm)
{
    if (va % PGSIZE != 0 || pa % PGSIZE != 0)
        panic("vm_mappages: va or pa not page aligned");
    if (len == 0)
        panic("vm_mappages: len zero");
    if (va + len > VA_MAX)
        panic("vm_mappages: va + len exceeds VA_MAX");

    uint64 end = va + len;
    uint64 cur_va = va;
    uint64 cur_pa = pa;
    while (cur_va < end) {
        pte_t* pte = vm_getpte(pgtbl, cur_va, true);
        if (*pte & PTE_V)
            panic("vm_mappages: remap");
        *pte = PA_TO_PTE(cur_pa) | perm | PTE_V;

        cur_va += PGSIZE;
        cur_pa += PGSIZE;
    }
}

// 解除pgtbl中[va, va+len)区域的映射
// 如果freeit == true则释放对应物理页, 默认是用户的物理页
void vm_unmappages(pgtbl_t pgtbl, uint64 va, uint64 len, bool freeit)
{
    if (va % PGSIZE != 0)
        panic("vm_unmappages: va not page aligned");
    if (len == 0)
        panic("vm_unmappages: len zero");
    if (va + len > VA_MAX)
        panic("vm_unmappages: va + len exceeds VA_MAX");

    uint64 end = va + len;
    uint64 cur_va = va;
    while (cur_va < end) {
        pte_t* pte = vm_getpte(pgtbl, cur_va, false);
        if (pte == NULL || !(*pte & PTE_V))
            panic("vm_unmappages: not mapped");
        if (PTE_CHECK(*pte))
            panic("vm_unmappages: not leaf");
        // 如果需要释放物理页
        if (freeit) {
            uint64 pa = PTE_TO_PA(*pte);
            // 这里释放时，物理页可能属于内核区或用户区，我们需要判断
            // 简单起见，我们使用一个辅助函数来判断区域归属，或根据上下文传入标志。
            // 这里统一当作内核区释放（因为当前只处理内核页表），实际应根据地址判断。
            // 但为了通用，我们可以根据地址范围判断：
            bool in_kernel = (pa >= (uint64)ALLOC_BEGIN && pa < (uint64)ALLOC_BEGIN + KERN_PAGES * PGSIZE);
            pmem_free(pa, in_kernel);
        }
        // 清除PTE
        *pte = 0;
        cur_va += PGSIZE;
    }
}

// 完成UART、CLINT、PLIC、内核代码区、内核数据区、可分配区域的页表映射
// 相当于部分填充kernel_pgtbl
void kvm_init()
{
    // 分配根页表
    kernel_pgtbl = (pgtbl_t)pmem_alloc(true);
    memset(kernel_pgtbl, 0, PGSIZE);

    // 映射 UART
    vm_mappages(kernel_pgtbl, UART_BASE, UART_BASE, PGSIZE, PTE_R | PTE_W);

    // 映射 CLINT (0x2000000, 大小 0x10000)
    vm_mappages(kernel_pgtbl, CLINT_BASE, CLINT_BASE, 0x10000, PTE_R | PTE_W);

    // 映射 PLIC (0x0c000000, 大小 0x400000)
    vm_mappages(kernel_pgtbl, PLIC_BASE, PLIC_BASE, 0x400000, PTE_R | PTE_W);

    // 映射内核代码段（从 KERNEL_BASE 到 KERNEL_DATA）
    vm_mappages(kernel_pgtbl, KERNEL_BASE, KERNEL_BASE,
        (uint64)KERNEL_DATA - KERNEL_BASE, PTE_R | PTE_X);

    // 映射内核数据段（从 KERNEL_DATA 到 ALLOC_BEGIN）
    vm_mappages(kernel_pgtbl, (uint64)KERNEL_DATA, (uint64)KERNEL_DATA,
        (uint64)ALLOC_BEGIN - (uint64)KERNEL_DATA, PTE_R | PTE_W);

    // 映射整个可分配区域（ALLOC_BEGIN ~ ALLOC_END），内核可读写
    // 注意：用户进程会建立自己的页表，但内核需要能直接访问物理内存，所以直接映射所有内存
    vm_mappages(kernel_pgtbl, (uint64)ALLOC_BEGIN, (uint64)ALLOC_BEGIN,
        (uint64)ALLOC_END - (uint64)ALLOC_BEGIN, PTE_R | PTE_W);

    // 映射 trampoline（用于S-mode和U-mode切换）
    // 在用户和内核页表中使用相同的虚拟地址(TRAMPOLINE)，指向同一个物理页面
    extern char trampoline[];
    vm_mappages(kernel_pgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

    // 映射虚拟磁盘 VIRTIO MMIO 寄存器区域
    vm_mappages(kernel_pgtbl, VIRTIO_BASE, VIRTIO_BASE, PGSIZE, PTE_R | PTE_W);

    // 分配并映射所有进程的内核栈 (每个进程一个内核栈页面)
    // KSTACK(procid) 的设计中, 相邻内核栈之间间隔一个 guard page (未映射, 防止栈溢出)
    for (int i = 0; i < N_PROC; i++) {
        uint64 kstack_pa = (uint64)pmem_alloc(true);
        vm_mappages(kernel_pgtbl, KSTACK(i), kstack_pa, PGSIZE, PTE_R | PTE_W);
    }

}

// 每个CPU都需要调用, 从不使用页表切换到使用内核页表
// 切换后需要刷新TLB里面的缓存
void kvm_inithart()
{
    w_satp(MAKE_SATP(kernel_pgtbl));
    sfence_vma();
}

// 返回内核页表，供进程模块使用
pgtbl_t kvm_pgtbl()
{
    return kernel_pgtbl;
}

// 输出页表内容(for debug)
void vm_print(pgtbl_t pgtbl)
{
    // 顶级页表，次级页表，低级页表
    pgtbl_t pgtbl_2 = pgtbl, pgtbl_1 = NULL, pgtbl_0 = NULL;
    pte_t pte;

    printf("level-2 pgtbl: pa = %x\n", pgtbl_2);
    for (int i = 0; i < PGSIZE / sizeof(pte_t); i++)
    {
        pte = pgtbl_2[i];
        if (!((pte)&PTE_V))
            continue;
        assert(PTE_CHECK(pte), "vm_print: pte check fail (1)");
        pgtbl_1 = (pgtbl_t)PTE_TO_PA(pte);
        printf(".. level-1 pgtbl %d: pa = %x\n", i, pgtbl_1);

        for (int j = 0; j < PGSIZE / sizeof(pte_t); j++)
        {
            pte = pgtbl_1[j];
            if (!((pte)&PTE_V))
                continue;
            assert(PTE_CHECK(pte), "vm_print: pte check fail (2)");
            pgtbl_0 = (pgtbl_t)PTE_TO_PA(pte);
            printf(".. .. level-0 pgtbl %d: pa = %x\n", j, pgtbl_0);

            for (int k = 0; k < PGSIZE / sizeof(pte_t); k++)
            {
                pte = pgtbl_0[k];
                if (!((pte)&PTE_V))
                    continue;
                assert(!PTE_CHECK(pte), "vm_print: pte check fail (3)");
                printf(".. .. .. physical page %d: pa = %x flags = %d\n", k, (uint64)PTE_TO_PA(pte), (int)PTE_FLAGS(pte));
            }
        }
    }
}
