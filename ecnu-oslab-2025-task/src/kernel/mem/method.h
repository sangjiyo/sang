#pragma once

/* pmem.c: 物理内存管理逻辑 */

void pmem_init(void);
void *pmem_alloc(bool in_kernel);
void pmem_free(uint64 page, bool in_kernel);

/* kvm.c: 内核态虚拟内存管理 + 页表通用函数 */

pte_t *vm_getpte(pgtbl_t pgtbl, uint64 va, bool alloc);
void vm_mappages(pgtbl_t pgtbl, uint64 va, uint64 pa, uint64 len, int perm);
void vm_unmappages(pgtbl_t pgtbl, uint64 va, uint64 len, bool freeit);
void vm_print(pgtbl_t pgtbl);
void kvm_init();
void kvm_inithart();
