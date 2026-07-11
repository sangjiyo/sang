#include "mod.h"

/*
	将ELF文件中的segment放入内存中制定位置
	inode逻辑区域: [seg_start, seg_start + len)
	进程地址空间: [va_start, va_start + len), 对应的物理页是存在的
*/
static void load_segment(inode_t *ip, pgtbl_t pgtbl, 
	uint64 seg_start, uint64 va_start, uint32 len)
{
	assert(va_start % PGSIZE == 0, "load_segment: va aligned!");

	pte_t *pte;
	uint64 pa;
	uint32 read_len, cut_len;

	for (read_len = 0; read_len < len; read_len += PGSIZE)
	{
		/* 获取物理内存地址 */
		pte = vm_getpte(pgtbl, va_start + read_len, false);
		pa = PTE_TO_PA(*pte);
		assert(pa != 0, "load_segment: invalid pa!");

		/* 读入segment的一部分 */
		cut_len = MIN(len - read_len, PGSIZE);
		if (inode_read_data(ip, (uint32)seg_start + read_len, cut_len, (void*)pa, false) != cut_len)
			panic("load_segment: read fail!");
	}
}

/* 将程序的代码区和数据区读入用户堆中, 返回new_heap_top */
static uint64 prepare_heap(pgtbl_t new_pgtbl, inode_t *ip, elf_header_t *eh)
{
	program_header_t ph;
	uint64 new_heap_top = USER_BASE, old_heap_top = USER_BASE;
	
	for (uint32 off = eh->ph_off; off < eh->ph_off + eh->ph_ent_num * sizeof(ph); off += sizeof(ph))
	{
		// 读入一个program header
		if (inode_read_data(ip, off, sizeof(ph), &ph, false) != sizeof(ph))
			return -1;
		
		// 判断是否有必要载入
		if (ph.type != ELF_PROG_LOAD)
			continue;
		
		// program header参数的合法性检查
		if (ph.mem_size < ph.file_size)
			return -1;
		if (ph.va + ph.mem_size < ph.va)
			return -1;
		if (ph.va % PGSIZE != 0)
			return -1;
		
		// 用户堆生长
		new_heap_top = uvm_heap_grow(new_pgtbl, old_heap_top,
						ph.va + ph.mem_size - old_heap_top, PTE_R | PTE_X);
		if (new_heap_top != ph.va + ph.mem_size)
			return -1;
		old_heap_top = new_heap_top;

		// segment读入
		load_segment(ip, new_pgtbl, ph.off, ph.va, ph.file_size);
	}

	return new_heap_top;
}

/* 准备栈空间用于存储输入参数(4KB), 设置arg_count, 返回sp */
static uint64 prepare_stack(pgtbl_t new_pgtbl, char **argv, int *arg_count)
{
	uint64 ustack_page;
	uint64 sp = TRAPFRAME, sp_base = TRAPFRAME - PGSIZE;
	uint64 sp_list[ELF_MAXARGS + 1];
	uint32 argc, arg_len;

	ustack_page = (uint64)pmem_alloc(false);
	vm_mappages(new_pgtbl, sp_base, ustack_page, PGSIZE, PTE_R | PTE_W | PTE_U);
	
	for (argc = 0; argv[argc] != NULL; argc++)
	{
		if (argc >= ELF_MAXARGS)
			return -1;
		
		arg_len = strlen(argv[argc]) + 1;
		sp -= ALIGN_UP(arg_len, 16);
		if (sp < sp_base)
			return -1;
		
		uvm_copyout(new_pgtbl, sp, (uint64)argv[argc], arg_len);

		sp_list[argc] = sp;
	}
	sp_list[argc] = 0;

	arg_len = (argc + 1) * sizeof(uint64);
	sp -= ALIGN_UP(arg_len, 16);
	if (sp < sp_base)
		return -1;

	uvm_copyout(new_pgtbl, sp, (uint64)sp_list, arg_len);

	*arg_count = argc;

	return sp;
}

/*
	执行ELF文件
	输入路径和参数
	成功返回argc, 失败返回-1
*/
int proc_exec(char *path, char **argv)
{
	
}
