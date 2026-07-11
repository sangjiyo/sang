#pragma once

typedef struct file file_t;

void syscall(void);

void arg_uint32(int n, uint32 *ip);
void arg_uint64(int n, uint64 *ip);
void arg_str(int n, char *buf, int maxlen);
int arg_fd(int n, uint32 *pfd, file_t **pfile);

uint64 sys_brk();
uint64 sys_mmap();
uint64 sys_munmap();
uint64 sys_print_str();
uint64 sys_print_int();
uint64 sys_fork();
uint64 sys_wait();
uint64 sys_exit();
uint64 sys_sleep();
uint64 sys_getpid();
uint64 sys_alloc_block();
uint64 sys_free_block();
uint64 sys_alloc_inode();
uint64 sys_free_inode();
uint64 sys_show_bitmap();
uint64 sys_get_block();
uint64 sys_put_block();
uint64 sys_read_block();
uint64 sys_write_block();
uint64 sys_show_buffer();
uint64 sys_flush_buffer();
uint64 sys_exec();
uint64 sys_open();
uint64 sys_close();
uint64 sys_read();
uint64 sys_write();
uint64 sys_lseek();
uint64 sys_dup();
uint64 sys_fstat();
uint64 sys_get_dentries();
uint64 sys_mkdir();
uint64 sys_chdir();
uint64 sys_print_cwd();
uint64 sys_link();
uint64 sys_unlink();