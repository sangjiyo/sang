#pragma once

// proc.c: 页表初始化 + 第一个进程初始化

pgtbl_t proc_pgtbl_init(uint64 trapframe);
void proc_make_first();
