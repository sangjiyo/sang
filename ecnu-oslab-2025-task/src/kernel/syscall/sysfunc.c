#include "mod.h"

/*
    用户堆空间伸缩
    uint64 new_heap_top (如果是0, 代表查询当前堆顶位置)
    成功返回new_heap_top, 失败返回-1
*/
uint64 sys_brk()
{
    uint64 new_heap_top;
    arg_uint64(0, &new_heap_top);

    proc_t* p = myproc();
    uint64 old_heap_top = p->heap_top;

    // 查询当前堆顶
    if (new_heap_top == 0) {
        return old_heap_top;
    }

    // 边界检查：堆不能超过 MMAP_BEGIN
    if (new_heap_top > MMAP_BEGIN) {
        return -1;
    }

    // 增长
    if (new_heap_top > old_heap_top) {
        uint32 inc = new_heap_top - old_heap_top;
        uint64 ret = uvm_heap_grow(p->pgtbl, old_heap_top, inc);
        p->heap_top = ret;
        return ret;
    }
    // 收缩
    if (new_heap_top < old_heap_top) {
        uint32 dec = old_heap_top - new_heap_top;
        uint64 ret = uvm_heap_ungrow(p->pgtbl, old_heap_top, dec);
        p->heap_top = ret;
        return ret;
    }

    // 相等
    return old_heap_top;
}

/*
    增加一段内存映射
    uint64 start 起始地址
    uint32 len   范围 (字节,需检查是否是page-aligned)
    成功返回映射空间的起始地址, 失败返回-1
*/
uint64 sys_mmap()
{
    uint64 begin;
    uint32 len;
    arg_uint64(0, &begin);
    arg_uint32(1, &len);
    uint32 npages = (len + PGSIZE - 1) / PGSIZE;
    if (len == 0) return -1;
    // 若 begin 不为0，需检查页对齐
    if (begin != 0 && (begin % PGSIZE) != 0) {
        return -1;  // 地址未页对齐
    }
    // 调用 uvm_mmap，内部会检查范围并自动分配/映射
    uvm_mmap(begin, npages, PTE_R | PTE_W);
    proc_t* p = myproc();
    mmap_region_t* last = p->mmap;
    while (last && last->next) last = last->next;
    if (begin == 0 && last) {
        return last->begin;
    }
    return begin;  // 如果begin非0，返回begin
}

/*
    解除一段内存映射
    uint64 start 起始地址
    uint32 len   范围 (字节, 需检查是否是page-aligned)
    成功返回0 失败返回-1
*/
uint64 sys_munmap()
{
    uint64 begin;
    uint32 len;
    arg_uint64(0, &begin);
    arg_uint32(1, &len);
    uint32 npages = (len + PGSIZE - 1) / PGSIZE;
    if (len == 0 || (begin % PGSIZE) != 0) {
        return -1;
    }
    uvm_munmap(begin, npages);
    return 0;
}

// 打印一个字符串
// char *str
// 成功返回0
uint64 sys_print_str()
{
    uint64 addr;
    arg_uint64(0, &addr);

    // 从用户空间读取字符串到内核缓冲区
    char buf[STR_MAXLEN + 1];
    proc_t *p = myproc();
    uvm_copyin_str(p->pgtbl, (uint64)buf, addr, STR_MAXLEN);

    // 打印字符串
    printf("%s", buf);
    return 0;
}

// 打印一个32位整数
// int num
// 成功返回0
uint64 sys_print_int()
{
    uint32 num;
    arg_uint32(0, &num);
    printf("num = %d\n", (int)num);
    return 0;
}

// 进程复制
// 返回子进程的pid
uint64 sys_fork()
{
    return proc_fork();
}

// 等待子进程退出
// uint64 addr_exit_state
uint64 sys_wait()
{
    uint64 addr;
    arg_uint64(0, &addr);
    return proc_wait(addr);
}

// 进程退出
// int exit_code
// 不返回
uint64 sys_exit()
{
    uint32 exit_code;
    arg_uint32(0, &exit_code);
    proc_exit((int)exit_code);
    // proc_exit不会返回, 这里只是为了编译器
    return 0;
}

// 让进程睡眠一段时间
// uint32 ntick (1个tick大约0.1秒)
// 成功返回0
uint64 sys_sleep()
{
    uint32 ntick;
    arg_uint32(0, &ntick);
    timer_wait((uint64)ntick);
    return 0;
}

// 返回当前进程的pid
uint64 sys_getpid()
{
    return myproc()->pid;
}

/*
    从data_bitmap申请1个block (测试data_bitmap_alloc)
    返回block序号
*/
uint64 sys_alloc_block()
{
    return bitmap_alloc_block();
}

/*
    向data_bitmap释放1个block (测试data_bitmap_free)
    uint32 block_num (目标block序号)
    成功返回0
*/
uint64 sys_free_block()
{
    uint32 block_num;
    arg_uint32(0, &block_num);
    bitmap_free_block(block_num);
    return 0;
}

/*
    从inode_bitmap申请1个inode (测试inode_bitmap_alloc)
    返回inode序号
*/
uint64 sys_alloc_inode()
{
    return bitmap_alloc_inode();
}

/*
    向inode_bitmap释放1个inode (测试inode_bitmap_free)
    uint32 inode_num (目标inode序号)
    成功返回0
*/
uint64 sys_free_inode()
{
    uint32 inode_num;
    arg_uint32(0, &inode_num);
    bitmap_free_inode(inode_num);
    return 0;
}

/*
    输出目标bitmap的状态
    uint32 choose_bitmap (0->data_bitmap 1->inode_bitmap)
    成功返回0, 失败返回-1
*/
uint64 sys_show_bitmap()
{
    uint32 choose;
    arg_uint32(0, &choose);
    if (choose > 1)
        return -1;
    bitmap_print(choose == 0 ? true : false);
    return 0;
}

/*
    获取1个描述block的buffer (测试buffer_get)
    uint32 block_num 目标block的序号
    成功返回buffer地址, 失败返回-1
*/
uint64 sys_get_block()
{
    uint32 block_num;
    arg_uint32(0, &block_num);
    buffer_t *buf = buffer_get(block_num);
    return (uint64)buf;
}

/*
    释放1个描述block的buffer (测试buffer_put)
    uint64 addr_buf 即将被释放的buffer
    成功返回0
*/
uint64 sys_put_block()
{
    uint64 addr_buf;
    arg_uint64(0, &addr_buf);
    buffer_put((buffer_t*)addr_buf);
    return 0;
}

/*
    将buf->data拷贝到用户空间 (测试buffer_read)
    uint64 addr_buf 使用的buffer
    uint64 addr_data 用户数据区 (copy dst)
    成功返回0
*/
uint64 sys_read_block()
{
    uint64 addr_buf, addr_data;
    arg_uint64(0, &addr_buf);
    arg_uint64(1, &addr_data);

    buffer_t *buf = (buffer_t*)addr_buf;
    proc_t *p = myproc();

    // 将buf->data拷贝到用户空间
    uvm_copyout(p->pgtbl, addr_data, (uint64)buf->data, BLOCK_SIZE);
    return 0;
}

/*
    将用户空间数据同步到内核空间, 并通过buffer写入block (测试buffer_write)
    uint64 addr_buf 使用的buffer
    uint64 addr_data 用户数据区 (copy src)
    成功返回0
*/
uint64 sys_write_block()
{
    uint64 addr_buf, addr_data;
    arg_uint64(0, &addr_buf);
    arg_uint64(1, &addr_data);

    buffer_t *buf = (buffer_t*)addr_buf;
    proc_t *p = myproc();

    // 将用户空间数据拷贝到buf->data
    uvm_copyin(p->pgtbl, (uint64)buf->data, addr_data, BLOCK_SIZE);

    // 写入磁盘
    buffer_write(buf);
    return 0;
}

/*
    输出buffer链表的状态
    成功返回0
*/
uint64 sys_show_buffer()
{
    buffer_print_info();
    return 0;
}

/*
    释放非活跃链表中buffer持有的物理内存资源
    uint32 buffer_count (希望释放的buffer数量)
    成功返回0
*/
uint64 sys_flush_buffer()
{
    uint32 buffer_count;
    arg_uint32(0, &buffer_count);
    buffer_freemem(buffer_count);
    return 0;
}

/*
    执行ELF文件以替换当前进程的内容
    char *path
    char **argv
    成功返回argc, 失败返回-1
*/
uint64 sys_exec()
{

}

/* 构建fd->file的映射, 返回fd */
static uint32 alloc_fd(file_t *file)
{
    proc_t *p = myproc();
    for (uint32 i = 0; i < N_OPEN_FILE_PER_PROC; i++)
    {
        if (p->open_file[i] == NULL) {
            p->open_file[i] = file;
            return i;
        }
    }
    return -1;
}

/*
    打开或创建文件
    char *path
    uint32 open_mode
    成功返回fd, 失败返回-1
*/
uint64 sys_open()
{

}

/*
    关闭文件
    uint32 fd
    成功返回0, 失败返回-1
*/
uint64 sys_close()
{

}

/*
    读取文件内容
    uint32 fd
    uint32 len
    uint64 addr
    成功返回读到的字节数, 失败返回0
*/
uint64 sys_read()
{

}

/*
    写入文件内容
    uint32 fd
    uint32 len
    uint64 addr
    成功返回写入的字节数, 失败返回0
*/
uint64 sys_write()
{

}

/*
    调整读写指针位置
    uint32 fd
    uint32 offset
    uint32 flag
    成功返回新的偏移量, 失败返回-1
*/
uint64 sys_lseek()
{

}

/*
    复制文件控制权
    uinr32 fd
    成功返回new_fd, 失败返回-1
*/
uint64 sys_dup()
{

}

/*
    获取文件信息
    uint32 fd
    uint64 addr
    成功返回0, 失败返回-1
*/
uint64 sys_fstat()
{

}

/*
    获取目录中的所有目录项信息
    uint32 fd
    uint64 addr
    uint32 buffer_len
    成功返回读到的字节数, 失败返回-1
*/
uint64 sys_get_dentries()
{

}

/*
    创建目录
    char *path
    成功返回0, 失败返回-1
*/
uint64 sys_mkdir()
{

}

/*
    修改当前工作目录
    char *new_path
    成功返回0, 失败返回-1
*/
uint64 sys_chdir()
{

}

/*
    打印当前工作目录的绝对路径
    成功返回0, 失败返回-1
*/
uint64 sys_print_cwd()
{

}

/*
    新建链接
    char *old_path
    char *new_path
    成功返回0, 失败返回-1
*/
uint64 sys_link()
{

}


/*
    删除链接 (可能触发删除文件)
    char *path
    成功返回0, 失败返回-1
*/
uint64 sys_unlink()
{

}