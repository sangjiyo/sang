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
        uint64 ret = uvm_heap_grow(p->pgtbl, old_heap_top, inc, PTE_R | PTE_W);
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
    执行ELF文件以替换当前进程的内容
    char *path
    char **argv
    成功返回argc, 失败返回-1
*/
uint64 sys_exec()
{
    uint64 path_addr, argv_addr;
    arg_uint64(0, &path_addr);
    arg_uint64(1, &argv_addr);

    char path[STR_MAXLEN + 1];
    uvm_copyin_str(myproc()->pgtbl, (uint64)path, path_addr, STR_MAXLEN);

    // 逐个读入argv指针, 避免一次读256字节跨页踩到未映射内存
    char* argv[ELF_MAXARGS + 1];
    proc_t* p = myproc();
    int argc;
    for (argc = 0; argc < ELF_MAXARGS; argc++) {
        uint64 uptr;
        uvm_copyin(p->pgtbl, (uint64)&uptr, argv_addr + argc * sizeof(uint64), sizeof(uint64));
        if (uptr == 0) break;
        argv[argc] = (char*)pmem_alloc(true);
        uvm_copyin_str(p->pgtbl, (uint64)argv[argc], uptr, ELF_MAXARG_LEN);
    }
    argv[argc] = NULL;

    int ret = proc_exec(path, argv);

    for (int i = 0; i < argc; i++)
        pmem_free((uint64)argv[i], true);

    return ret;
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
    uint64 path_addr;
    uint32 open_mode;
    arg_uint64(0, &path_addr);
    arg_uint32(1, &open_mode);

    char path[STR_MAXLEN + 1];
    uvm_copyin_str(myproc()->pgtbl, (uint64)path, path_addr, STR_MAXLEN);

    file_t *file = file_open(path, open_mode);
    if (file == NULL)
        return -1;

    return alloc_fd(file);
}

/*
    关闭文件
    uint32 fd
    成功返回0, 失败返回-1
*/
uint64 sys_close()
{
    uint32 fd;
    file_t *file;
    if (arg_fd(0, &fd, &file) < 0)
        return -1;

    myproc()->open_file[fd] = NULL;
    file_close(file);
    return 0;
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
    uint32 fd, len;
    uint64 addr;
    file_t *file;
    if (arg_fd(0, &fd, &file) < 0)
        return -1;
    arg_uint32(1, &len);
    arg_uint64(2, &addr);

    return file_read(file, len, addr, true);
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
    uint32 fd, len;
    uint64 addr;
    file_t* file;
    if (arg_fd(0, &fd, &file) < 0)
        return -1;
    arg_uint32(1, &len);
    arg_uint64(2, &addr);

    return file_write(file, len, addr, true);
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
    uint32 fd, offset, flag;
    file_t *file;
    if (arg_fd(0, &fd, &file) < 0)
        return -1;
    arg_uint32(1, &offset);
    arg_uint32(2, &flag);

    return file_lseek(file, offset, flag);
}

/*
    复制文件控制权
    uinr32 fd
    成功返回new_fd, 失败返回-1
*/
uint64 sys_dup()
{
    uint32 fd;
    file_t *file;
    if (arg_fd(0, &fd, &file) < 0)
        return -1;

    file_dup(file);
    return alloc_fd(file);
}

/*
    获取文件信息
    uint32 fd
    uint64 addr
    成功返回0, 失败返回-1
*/
uint64 sys_fstat()
{
    uint32 fd;
    uint64 addr;
    file_t *file;
    if (arg_fd(0, &fd, &file) < 0)
        return -1;
    arg_uint64(1, &addr);

    return file_get_stat(file, addr);
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
    uint32 fd, len;
    uint64 addr;
    file_t *file;
    if (arg_fd(0, &fd, &file) < 0)
        return -1;
    arg_uint64(1, &addr);
    arg_uint32(2, &len);

    return file_read(file, len, addr, true);
}

/*
    创建目录
    char *path
    成功返回0, 失败返回-1
*/
uint64 sys_mkdir()
{
    uint64 path_addr;
    arg_uint64(0, &path_addr);

    char path[STR_MAXLEN + 1];
    uvm_copyin_str(myproc()->pgtbl, (uint64)path, path_addr, STR_MAXLEN);

    inode_t *ip = path_create_inode(path, INODE_TYPE_DIR,
                                     INODE_MAJOR_DEFAULT, INODE_MINOR_DEFAULT);
    if (ip == NULL)
        return -1;

    inode_put(ip);
    return 0;
}

/*
    修改当前工作目录
    char *new_path
    成功返回0, 失败返回-1
*/
uint64 sys_chdir()
{
    uint64 path_addr;
    arg_uint64(0, &path_addr);

    char path[STR_MAXLEN + 1];
    uvm_copyin_str(myproc()->pgtbl, (uint64)path, path_addr, STR_MAXLEN);

    inode_t *ip = path_to_inode(path);
    if (ip == NULL)
        return -1;

    inode_lock(ip);
    if (ip->disk_info.type != INODE_TYPE_DIR) {
        inode_unlock(ip);
        inode_put(ip);
        return -1;
    }
    inode_unlock(ip);

    proc_t *p = myproc();
    if (p->cwd != NULL)
        inode_put(p->cwd);
    p->cwd = ip;
    return 0;
}

/*
    打印当前工作目录的绝对路径
    成功返回0, 失败返回-1
*/
uint64 sys_print_cwd()
{
    char path[STR_MAXLEN + 1];
    proc_t *p = myproc();

    if (p->cwd == NULL)
        return -1;

    uint32 offset = inode_to_path(p->cwd, path, STR_MAXLEN);

    if (offset == (uint32)-1)
        return -1;

    printf("current work directory = %s\n", path + offset);
    return 0;
}

/*
    新建链接
    char *old_path
    char *new_path
    成功返回0, 失败返回-1
*/
uint64 sys_link()
{
    uint64 old_addr, new_addr;
    arg_uint64(0, &old_addr);
    arg_uint64(1, &new_addr);

    char old_path[STR_MAXLEN + 1], new_path[STR_MAXLEN + 1];
    proc_t *p = myproc();
    uvm_copyin_str(p->pgtbl, (uint64)old_path, old_addr, STR_MAXLEN);
    uvm_copyin_str(p->pgtbl, (uint64)new_path, new_addr, STR_MAXLEN);

    return path_link(old_path, new_path);
}


/*
    删除链接 (可能触发删除文件)
    char *path
    成功返回0, 失败返回-1
*/
uint64 sys_unlink()
{
    uint64 path_addr;
    arg_uint64(0, &path_addr);

    char path[STR_MAXLEN + 1];
    uvm_copyin_str(myproc()->pgtbl, (uint64)path, path_addr, STR_MAXLEN);

    return path_unlink(path);
}