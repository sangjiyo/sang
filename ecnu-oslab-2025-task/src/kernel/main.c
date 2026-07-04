#include "arch/mod.h"
#include "lib/mod.h"
#include "mem/mod.h"

// 全局标志：用于主核通知从核初始化已完成
volatile static int started = 0;

volatile static int over_1 = 0, over_2 = 0;


/*---------------------------------- 测试代码 --------------------------------*/

void test_mapping_and_unmapping()
{
    // 1. 初始化测试页表
    pte_t* pte;
    pgtbl_t pgtbl = (pgtbl_t)pmem_alloc(true);
    memset(pgtbl, 0, PGSIZE);

    // 2. 准备测试条件
    uint64 va_1 = 0x100000;
    uint64 va_2 = 0x8000;
    uint64 pa_1 = (uint64)pmem_alloc(false);
    uint64 pa_2 = (uint64)pmem_alloc(false);

    // 3. 建立映射
    vm_mappages(pgtbl, va_1, pa_1, PGSIZE, PTE_R | PTE_W);
    vm_mappages(pgtbl, va_2, pa_2, PGSIZE, PTE_R);

    // 4. 验证映射结果
    pte = vm_getpte(pgtbl, va_1, false);
    assert(pte != NULL, "test_mapping_and_unmapping: pte_1 not found");
    assert((*pte & PTE_V) != 0, "test_mapping_and_unmapping: pte_1 not valid");
    assert(PTE_TO_PA(*pte) == pa_1, "test_mapping_and_unmapping: pa_1 mismatch");
    assert((*pte & (PTE_R | PTE_W)) == (PTE_R | PTE_W), "test_mapping_and_unmapping: flag_1 mismatch");

    pte = vm_getpte(pgtbl, va_2, false);
    assert(pte != NULL, "test_mapping_and_unmapping: pte_2 not found");
    assert((*pte & PTE_V) != 0, "test_mapping_and_unmapping: pte_2 not valid");
    assert(PTE_TO_PA(*pte) == pa_2, "test_mapping_and_unmapping: pa_2 mismatch");
    assert((*pte & (PTE_R == PTE_R)), "test_mapping_and_unmapping: flag_2 mismatch");

    // 5. 解除映射
    vm_unmappages(pgtbl, va_1, PGSIZE, true);
    vm_unmappages(pgtbl, va_2, PGSIZE, true);

    // 6. 验证解除映射结果
    pte = vm_getpte(pgtbl, va_1, false);
    assert(pte != NULL, "test_mapping_and_unmapping: pte_1 not found");
    assert((*pte & PTE_V) == 0, "test_mapping_and_unmapping: pte_1 still valid");
    pte = vm_getpte(pgtbl, va_2, false);
    assert(pte != NULL, "test_mapping_and_unmapping: pte_2 not found");
    assert((*pte & PTE_V) == 0, "test_mapping_and_unmapping: pte_2 still valid");

    // 7. 由于页表的释放函数还没实现, 作为测试用例可以展示不释放页表空间

    printf("test_mapping_and_unmapping passed!\n");
}

int main()
{
    int cpuid = r_tp();

    if (cpuid == 0) {

        print_init();
        pmem_init();
        kvm_init();
        kvm_inithart();

        printf("cpu %d is booting!\n", cpuid);
        __sync_synchronize();
        // started = 1;

        test_mapping_and_unmapping();

    }
    else {

        while (started == 0);
        __sync_synchronize();
        printf("cpu %d is booting!\n", cpuid);

    }
    while (1);
}