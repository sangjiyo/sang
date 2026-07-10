#include "arch/mod.h"
#include "lib/mod.h"
#include "mem/mod.h"
#include "trap/mod.h"
#include "proc/mod.h"

void proc_make_first(void);   // 添加这行显式声明

volatile static int started = 0;

// ---------- 任务5 测试函数 ----------
static void test_pgtbl_ops()
{
    printf("\n=== Testing page table copy and destroy ===\n");

    // 创建根页表
    pgtbl_t pgtbl = (pgtbl_t)pmem_alloc(true);
    memset(pgtbl, 0, PGSIZE);

    // 分配物理页并映射
    uint64 pa1 = (uint64)pmem_alloc(false);
    uint64 pa2 = (uint64)pmem_alloc(false);
    uint64 pa3 = (uint64)pmem_alloc(false);
    uint64 trapframe_pa = (uint64)pmem_alloc(true);   // 原 TRAPFRAME 物理页

    vm_mappages(pgtbl, USER_BASE, pa1, PGSIZE, PTE_R | PTE_X | PTE_U);
    vm_mappages(pgtbl, USER_BASE + PGSIZE, pa2, PGSIZE, PTE_R | PTE_W | PTE_U);
    vm_mappages(pgtbl, USER_BASE + 2 * PGSIZE, pa3, PGSIZE, PTE_R | PTE_W | PTE_U);
    vm_mappages(pgtbl, TRAPFRAME, trapframe_pa, PGSIZE, PTE_R | PTE_W);

    extern char trampoline[];
    vm_mappages(pgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

    printf("Original page table:\n");
    vm_print(pgtbl);

    // 创建副本页表
    pgtbl_t copy_pgtbl = (pgtbl_t)pmem_alloc(true);
    memset(copy_pgtbl, 0, PGSIZE);

    // 复制用户区域 (USER_BASE ~ 栈顶)
    printf("Copying page table...\n");
    uvm_copy_pgtbl(pgtbl, copy_pgtbl, USER_BASE + 2 * PGSIZE, 1, NULL);

    // 为副本单独分配 TRAPFRAME 物理页，并映射 TRAMPOLINE（共用内核页）
    uint64 trapframe_pa_copy = (uint64)pmem_alloc(true);
    vm_mappages(copy_pgtbl, TRAPFRAME, trapframe_pa_copy, PGSIZE, PTE_R | PTE_W);
    vm_mappages(copy_pgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

    printf("Copied page table:\n");
    vm_print(copy_pgtbl);

    // 销毁副本（会释放 trapframe_pa_copy，但不会释放 trampoline 物理页）
    printf("Destroying copied page table...\n");
    uvm_destroy_pgtbl(copy_pgtbl);
    printf("Destroyed successfully.\n");

    // 销毁原页表（释放 trapframe_pa 等）
    uvm_destroy_pgtbl(pgtbl);
    printf("Original page table destroyed.\n");

    printf("=== Test completed ===\n\n");
}
// -------------------------------------


int main()
{
    int cpuid = r_tp();

    if (cpuid == 0) {

        print_init();
        printf("cpu %d is booting!\n", cpuid);

        pmem_init();
        kvm_init();
        kvm_inithart();
        trap_kernel_init();
        trap_kernel_inithart();
        mmap_init();

        // ---------- 运行任务5 测试 ----------
        test_pgtbl_ops();
        // -----------------------------------

        proc_make_first();

        __sync_synchronize();
        started = 1;
        
    } else {

        while (started == 0);
        __sync_synchronize();
        printf("cpu %d is booting!\n", cpuid);
        kvm_inithart();
        trap_kernel_inithart();
        

    }
    while (1);
}