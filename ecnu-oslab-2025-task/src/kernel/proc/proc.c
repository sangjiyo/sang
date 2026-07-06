#include "mod.h"

// 这个文件通过make build生成, 是proczero对应的ELF文件
//#include "../../user/initcode.h"
//#define initcode target_user_initcode
//#define initcode_len target_user_initcode_len

// in trampoline.S
extern char trampoline[];
extern char user_vector[];
extern char user_return[];

// in swtch.S
extern void swtch(context_t *old, context_t *new);

// in trap/trap_user.c
extern void trap_user_return();

// 第一个用户进程
static proc_t proczero;

// 获得一个初始化过的用户页表
// 完成trapframe和trampoline的映射
pgtbl_t proc_pgtbl_init(uint64 trapframe)
{
    // 创建空页表（根页表）
    pgtbl_t pgtbl = (pgtbl_t)pmem_alloc(true);
    memset(pgtbl, 0, PGSIZE);

    // 映射 trampoline（用户和内核共享的高地址页）
    // 注意：trampoline 物理地址为 (uint64)trampoline，虚拟地址为 TRAMPOLINE
    // 映射 trampoline（用户态需要执行代码，加 U 和 X）
    vm_mappages(pgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X | PTE_U);

    // 映射 trapframe 页（位于 TRAPFRAME 虚拟地址）
    // 映射 trapframe（用户态需要读写，加 U）
    vm_mappages(pgtbl, TRAPFRAME, (uint64)trapframe, PGSIZE, PTE_R | PTE_W | PTE_U);

    return pgtbl;
}

/*
    第一个用户态进程的创建
    它的代码和数据位于initcode.h的initcode数组

    第一个进程的用户地址空间布局:
    trapoline   (1 page)
    trapframe   (1 page)
    ustack      (1 page)
    .......
                        <--heap_top
    code + data (1 page)
    empty space (1 page) 最低的4096字节 不分配物理页，同时不可访问

	注意: 用用户空间的地址映射需要标记 PTE_U
*/
void proc_make_first()
{
    // 1. 分配用户物理页并拷贝 initcode
    uint64 code_pa = (uint64)pmem_alloc(false);   // 用户空间页
    //memmove((void*)code_pa, initcode, initcode_len);

    // 使用硬编码测试代码替代 initcode
    unsigned char test_code[] = {
        0x93, 0x08, 0x00, 0x00,   // li a7, 0
        0x73, 0x00, 0x00, 0x00,   // ecall
        0x6f, 0x00, 0x00, 0x00,   // j 0
    };
    memmove((void*)code_pa, test_code, sizeof(test_code));
    // 在 memmove 之后，memset 之前
    asm volatile("fence.i" ::: "memory");

    // 剩余页清零
    //memset((void*)(code_pa + initcode_len), 0, PGSIZE - initcode_len);
    memset((void*)(code_pa + sizeof(test_code)), 0, PGSIZE - sizeof(test_code));

    // 2. 分配用户栈页
    uint64 stack_pa = (uint64)pmem_alloc(false);
    memset((void*)stack_pa, 0, PGSIZE);

    // 3. 分配 trapframe 物理页（内核可访问）
    uint64 tf_pa = (uint64)pmem_alloc(true);
    trapframe_t* tf = (trapframe_t*)tf_pa;
    memset(tf, 0, PGSIZE);

    // 4. 分配内核栈物理页并映射到内核页表
    uint64 kstack_pa = (uint64)pmem_alloc(true);
    uint64 kstack_va = KSTACK(0);   // 第一个进程使用 CPU0 的内核栈（或其他）
    kvmmap(kstack_va, kstack_pa, PGSIZE, PTE_R | PTE_W);

    // 5. 创建用户页表
    pgtbl_t pgtbl = proc_pgtbl_init(tf_pa);

    // 6. 映射代码页到虚拟地址 0
    vm_mappages(pgtbl, 0, code_pa, PGSIZE, PTE_R | PTE_X | PTE_U);

    // 7. 映射用户栈页到虚拟地址（例如 PGSIZE * 2，或计算堆顶）
    // 这里简单将栈放在代码页之后一页
    uint64 ustack_va = PGSIZE;   // 或者从 PGSIZE 开始，但为了安全可以更高
    vm_mappages(pgtbl, ustack_va, stack_pa, PGSIZE, PTE_R | PTE_W | PTE_U);

    // 8. 填充 trapframe
    tf->user_to_kern_satp = MAKE_SATP(kernel_pgtbl);
    tf->user_to_kern_sp = kstack_va + PGSIZE;   // 内核栈顶（向下增长）
    tf->user_to_kern_trapvector = (uint64)trap_user_handler;
    tf->user_to_kern_hartid = r_tp();               // 当前 hartid
    tf->user_to_kern_epc = 0;                    // 从地址 0 开始执行
    tf->sp = ustack_va + PGSIZE;   // 用户栈顶
    tf->a0 = TRAPFRAME;   // 用户态下 trapframe 的虚拟地址

    // 9. 初始化进程结构
    proczero.pid = 0;
    proczero.pgtbl = pgtbl;
    proczero.heap_top = PGSIZE;                 // 简单设定堆顶为代码页后
    proczero.ustack_npage = 1;
    proczero.tf = tf;
    proczero.kstack = kstack_va;
    // 上下文初始化（用于调度）
    memset(&proczero.ctx, 0, sizeof(context_t));
    proczero.ctx.ra = (uint64)trap_user_return;   // 首次调度后返回用户态
    proczero.ctx.sp = kstack_va + PGSIZE;          // 内核栈顶

    // 10. 设置当前 CPU 运行该进程
    mycpu()->proc = &proczero;

    // 切换到用户态（在 main.c 中调用 trap_user_return 触发）
}