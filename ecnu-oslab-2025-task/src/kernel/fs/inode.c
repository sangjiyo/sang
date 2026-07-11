#include "mod.h"

extern super_block_t sb;

/* 内存中的inode资源集合 */
static inode_t inode_cache[N_INODE];
static spinlock_t lk_inode_cache;

/* inode_cache初始化 */
void inode_init()
{
    spinlock_init(&lk_inode_cache, "lk_inode_cache");
    for (int i = 0; i < N_INODE; i++) {
        inode_t* ip = &inode_cache[i];
        ip->valid_info = false;
        ip->inode_num = INVALID_INODE_NUM;
        ip->ref = 0;
        sleeplock_init(&ip->slk, "inode");
        memset(&ip->disk_info, 0, sizeof(inode_disk_t));
    }
}

/*--------------------关于inode->index的增删查操作-----------------*/

/*
    供free_data_blocks使用
    递归删除inode->index中的一个元素
    返回删除过程中是否遇到空的block_num (文件末尾)
*/
static bool __free_data_blocks(uint32 block_num, uint32 level)
{
    // 如果block_num为0, 说明到达文件末尾, 返回true
    if (block_num == 0)
        return true;

    if (level == 0) {
        // 直接映射: 释放data block
        bitmap_free_block(block_num);
        return false;
    }

    // 间接映射: 读取index block, 递归释放其中的每个block
    buffer_t* buf = buffer_get(block_num);
    uint32* indices = (uint32*)buf->data;
    bool meet_empty = false;

    for (int i = 0; i < BLOCK_SIZE / sizeof(uint32); i++) {
        meet_empty = __free_data_blocks(indices[i], level - 1);
        if (meet_empty)
            break;
    }

    buffer_put(buf);

    // 释放index block本身
    bitmap_free_block(block_num);
    // 向上层传递是否遇到空block (用于free_data_blocks判断是否到达文件末尾)
    return meet_empty;
}

/*
    释放inode管理的blocks
*/
static void free_data_blocks(uint32* inode_index)
{
    unsigned int i;
    bool meet_empty = false;

    /* step-1: 释放直接映射的block */
    for (i = 0; i < INODE_INDEX_1; i++)
    {
        meet_empty = __free_data_blocks(inode_index[i], 0);
        if (meet_empty) return;
    }

    /* step-2: 释放一级间接映射的block */
    for (; i < INODE_INDEX_2; i++)
    {
        meet_empty = __free_data_blocks(inode_index[i], 1);
        if (meet_empty) return;
    }

    /* step-3: 释放二级间接映射的block */
    for (; i < INODE_INDEX_3; i++)
    {
        meet_empty = __free_data_blocks(inode_index[i], 2);
        if (meet_empty) return;
    }

    panic("free_data_blocks: impossible!");
}

/*
    获取inode第logical_block_num个block的物理序号block_num
    调用者保证输入的logical_block_num只有两种情况:
    1. 属于已经分配的区域 (返回block_num)
    2. 将已经分配出去的区域往外扩展1个block (申请block并返回block_num)
    成功返回block_num, 失败返回-1
*/
static uint32 locate_or_add_block(uint32* inode_index, uint32 logical_block_num)
{
    uint32 index_block_num, data_block_num;
    buffer_t* buf;
    uint32* indices;

    // 情况1: 直接映射区域 [0, INODE_BLOCK_INDEX_1)
    if (logical_block_num < INODE_BLOCK_INDEX_1) {
        uint32 i = logical_block_num; // 直接映射, 索引就是logical_block_num

        if (inode_index[i] == 0) {
            // 需要新分配
            inode_index[i] = bitmap_alloc_block();
        }
        return inode_index[i];
    }

    // 情况2: 一级间接映射区域 [INODE_BLOCK_INDEX_1, INODE_BLOCK_INDEX_2)
    if (logical_block_num < INODE_BLOCK_INDEX_2) {
        uint32 offset = logical_block_num - INODE_BLOCK_INDEX_1;
        uint32 idx_i = INODE_INDEX_1 + offset / (BLOCK_SIZE / sizeof(uint32));
        uint32 idx_j = offset % (BLOCK_SIZE / sizeof(uint32));

        if (inode_index[idx_i] == 0) {
            // 需要新分配index block, 并清零
            inode_index[idx_i] = bitmap_alloc_block();
            buf = buffer_get(inode_index[idx_i]);
            memset(buf->data, 0, BLOCK_SIZE);
            buffer_write(buf);
            buffer_put(buf);
        }

        buf = buffer_get(inode_index[idx_i]);
        indices = (uint32*)buf->data;

        if (indices[idx_j] == 0) {
            indices[idx_j] = bitmap_alloc_block();
            buffer_write(buf);
        }

        data_block_num = indices[idx_j];
        buffer_put(buf);
        return data_block_num;
    }

    // 情况3: 二级间接映射区域 [INODE_BLOCK_INDEX_2, INODE_MAX_SIZE/BLOCK_SIZE)
    if (logical_block_num < INODE_MAX_SIZE / BLOCK_SIZE) {
        uint32 offset = logical_block_num - INODE_BLOCK_INDEX_2;
        uint32 idx_i = INODE_INDEX_2 + offset / (BLOCK_SIZE / sizeof(uint32) * BLOCK_SIZE / sizeof(uint32));
        uint32 remaining = offset % (BLOCK_SIZE / sizeof(uint32) * BLOCK_SIZE / sizeof(uint32));
        uint32 idx_j = remaining / (BLOCK_SIZE / sizeof(uint32));
        uint32 idx_k = remaining % (BLOCK_SIZE / sizeof(uint32));

        // 确保一级index block存在
        if (inode_index[idx_i] == 0) {
            inode_index[idx_i] = bitmap_alloc_block();
            buf = buffer_get(inode_index[idx_i]);
            memset(buf->data, 0, BLOCK_SIZE);
            buffer_write(buf);
            buffer_put(buf);
        }

        // 读取一级index block
        buf = buffer_get(inode_index[idx_i]);
        uint32* level1 = (uint32*)buf->data;

        // 确保二级index block存在
        if (level1[idx_j] == 0) {
            level1[idx_j] = bitmap_alloc_block();
            buffer_write(buf);

            // 初始化二级index block
            buffer_t* buf2 = buffer_get(level1[idx_j]);
            memset(buf2->data, 0, BLOCK_SIZE);
            buffer_write(buf2);
            buffer_put(buf2);
        }

        index_block_num = level1[idx_j];
        buffer_put(buf);

        // 读取二级index block
        buf = buffer_get(index_block_num);
        uint32* level2 = (uint32*)buf->data;

        // 确保data block存在
        if (level2[idx_k] == 0) {
            level2[idx_k] = bitmap_alloc_block();
            buffer_write(buf);
        }

        data_block_num = level2[idx_k];
        buffer_put(buf);
        return data_block_num;
    }

    return -1; // logical_block_num超出范围
}

/*---------------------关于inode的管理: get dup lock unlock put----------------------*/

/*
    磁盘里的inode <-> 内存里的inode
    调用者需要持有ip->slk并设置合理的inode_num
*/
void inode_rw(inode_t* ip, bool write)
{
    // 计算inode在磁盘中的位置
    uint32 block_num = sb.inode_firstblock + ip->inode_num / INODE_PER_BLOCK;
    uint32 offset_in_block = (ip->inode_num % INODE_PER_BLOCK) * sizeof(inode_disk_t);

    buffer_t* buf = buffer_get(block_num);
    inode_disk_t* disk_inode = (inode_disk_t*)(buf->data + offset_in_block);

    if (write) {
        // 内存 -> 磁盘
        memmove(disk_inode, &ip->disk_info, sizeof(inode_disk_t));
        buffer_write(buf);
        ip->valid_info = true;
    }
    else {
        // 磁盘 -> 内存
        memmove(&ip->disk_info, disk_inode, sizeof(inode_disk_t));
        ip->valid_info = true;
    }

    buffer_put(buf);
}

/*
    尝试在inode_cache里寻找是否存在目标inode
    如果不存在则申请一个空闲的inode
    如果没有空闲位置直接panic
    核心逻辑: ref++
*/
inode_t* inode_get(uint32 inode_num)
{
    inode_t* empty = NULL;

    spinlock_acquire(&lk_inode_cache);

    // 先在cache中查找
    for (int i = 0; i < N_INODE; i++) {
        inode_t* ip = &inode_cache[i];

        if (ip->ref > 0 && ip->inode_num == inode_num) {
            // 找到了! ref++ 并返回 (不持有slk, 由调用者通过inode_lock获取)
            ip->ref++;
            spinlock_release(&lk_inode_cache);
            return ip;
        }

        // 记录第一个空闲位置
        if (empty == NULL && ip->ref == 0)
            empty = ip;
    }

    // 没找到, 使用空闲位置
    if (empty == NULL)
        panic("inode_get: no free inode in cache");

    empty->inode_num = inode_num;
    empty->ref = 1;
    empty->valid_info = false;
    spinlock_release(&lk_inode_cache);

    // 注意: 不持有slk返回, 调用者需通过inode_lock获取slk并读入磁盘信息
    return empty;
}

/*
    在磁盘里创建1个新的inode
    1. 查询和修改inode_bitmap
    2. 填充inode_region对应位置的inode
    注意: 返回的inode未上锁
*/
inode_t* inode_create(uint16 type, uint16 major, uint16 minor)
{
    // 申请inode序号
    uint32 inode_num = bitmap_alloc_inode();

    // 在cache中获取
    inode_t* ip = inode_get(inode_num);

    // 获取睡眠锁以独占访问
    sleeplock_acquire(&ip->slk);

    // 填充inode信息
    ip->disk_info.type = type;
    ip->disk_info.major = major;
    ip->disk_info.minor = minor;
    ip->disk_info.nlink = 1;
    ip->disk_info.size = 0;
    memset(ip->disk_info.index, 0, sizeof(ip->disk_info.index));

    // 写回磁盘
    inode_rw(ip, true);

    // 解锁后返回 (调用者需要自己加锁)
    sleeplock_release(&ip->slk);

    return ip;
}

/*
    ip->ref++ with lock protect
*/
inode_t* inode_dup(inode_t* ip)
{
    spinlock_acquire(&lk_inode_cache);
    ip->ref++;
    spinlock_release(&lk_inode_cache);
    return ip;
}

/*
    锁住inode
    如果inode->disk_info无效则更新一波
*/
void inode_lock(inode_t* ip)
{
    sleeplock_acquire(&ip->slk);
    // 如果info无效, 从磁盘读入
    if (!ip->valid_info) {
        inode_rw(ip, false);
    }
}

/*
    解锁inode
*/
void inode_unlock(inode_t* ip)
{
    sleeplock_release(&ip->slk);
}

/*
    与inode_get相对应, 调用者释放inode资源
    如果达成某些条件, 可能触发彻底删除
*/
void inode_put(inode_t* ip)
{
    spinlock_acquire(&lk_inode_cache);

    if (ip->ref > 0)
        ip->ref--;

    // 如果没有引用且nlink为0, 触发删除
    if (ip->ref == 0 && ip->disk_info.nlink == 0) {
        // 需要先释放锁, 因为inode_delete可能涉及磁盘操作
        spinlock_release(&lk_inode_cache);

        // 获取slk以确保独占访问
        sleeplock_acquire(&ip->slk);
        inode_delete(ip);
        sleeplock_release(&ip->slk);

        // 重新获取lk_inode_cache以重置inode
        spinlock_acquire(&lk_inode_cache);
        ip->inode_num = INVALID_INODE_NUM;
        ip->valid_info = false;
    }

    spinlock_release(&lk_inode_cache);
}

/*
    在磁盘里删除1个inode
    1. 修改inode_bitmap释放inode_region资源
    2. 修改block_bitmap释放block_region资源
    注意: 调用者需要持有ip->slk
*/
void inode_delete(inode_t* ip)
{
    // 1. 释放inode管理的所有data blocks
    free_data_blocks(ip->disk_info.index);

    // 2. 清零inode在磁盘中的位置
    ip->disk_info.type = 0;
    ip->disk_info.major = 0;
    ip->disk_info.minor = 0;
    ip->disk_info.nlink = 0;
    ip->disk_info.size = 0;
    memset(ip->disk_info.index, 0, sizeof(ip->disk_info.index));

    // 写回磁盘 (清零)
    inode_rw(ip, true);

    // 3. 释放inode slot
    bitmap_free_inode(ip->inode_num);

    ip->valid_info = false;
    ip->inode_num = INVALID_INODE_NUM;
}

/*----------------------基于inode的数据读写操作--------------------*/

/*
    基于inode的数据读取
    inode管理的数据空间逻辑上是一个连续的数组data
    需要拷贝data[offset,offset+len)到dst(用户态地址/内核态地址)
    返回读取的数据量(字节)
*/
uint32 inode_read_data(inode_t* ip, uint32 offset, uint32 len, void* dst, bool is_user_dst)
{
    // 边界检查
    if (offset + len > ip->disk_info.size)
        len = ip->disk_info.size - offset;
    if (len == 0)
        return 0;

    uint32 total_read = 0;
    uint32 remaining = len;
    uint32 cur_offset = offset;

    while (remaining > 0) {
        // 计算当前逻辑块号
        uint32 logical_block = cur_offset / BLOCK_SIZE;
        uint32 block_offset = cur_offset % BLOCK_SIZE;

        // 获取物理块号
        uint32 block_num = locate_or_add_block(ip->disk_info.index, logical_block);

        // 读取该block
        buffer_t* buf = buffer_get(block_num);

        // 计算本次拷贝的大小
        uint32 copy_len = BLOCK_SIZE - block_offset;
        if (copy_len > remaining)
            copy_len = remaining;

        // 拷贝数据到目标
        if (is_user_dst) {
            proc_t* p = myproc();
            uvm_copyout(p->pgtbl, (uint64)dst + total_read,
                (uint64)(buf->data + block_offset), copy_len);
        }
        else {
            memmove((void*)((uint64)dst + total_read),
                buf->data + block_offset, copy_len);
        }

        buffer_put(buf);

        remaining -= copy_len;
        cur_offset += copy_len;
        total_read += copy_len;
    }

    return total_read;
}

/*
    基于inode的数据写入
    inode管理的数据空间逻辑上是一个连续的数组data
    需要拷贝src(用户态地址/内核态地址)到data[offset,offset+len)
    返回写入的数据量(字节)
*/
uint32 inode_write_data(inode_t* ip, uint32 offset, uint32 len, void* src, bool is_user_src)
{
    // 文件大小不能超过上限
    if ((uint64)offset + len > INODE_MAX_SIZE)
        return -1;

    uint32 total_write = 0;
    uint32 remaining = len;
    uint32 cur_offset = offset;

    while (remaining > 0) {
        // 计算当前逻辑块号
        uint32 logical_block = cur_offset / BLOCK_SIZE;
        uint32 block_offset = cur_offset % BLOCK_SIZE;

        // 获取或分配物理块号
        uint32 block_num = locate_or_add_block(ip->disk_info.index, logical_block);
        if (block_num == (uint32)-1)
            break;

        // 读取该block (写入可能需要read-modify-write)
        buffer_t* buf = buffer_get(block_num);

        // 计算本次拷贝的大小
        uint32 copy_len = BLOCK_SIZE - block_offset;
        if (copy_len > remaining)
            copy_len = remaining;

        // 从源拷贝数据
        if (is_user_src) {
            proc_t* p = myproc();
            uvm_copyin(p->pgtbl, (uint64)(buf->data + block_offset),
                (uint64)src + total_write, copy_len);
        }
        else {
            memmove(buf->data + block_offset,
                (void*)((uint64)src + total_write), copy_len);
        }

        // 写回磁盘
        buffer_write(buf);
        buffer_put(buf);

        remaining -= copy_len;
        cur_offset += copy_len;
        total_write += copy_len;
    }

    // 更新文件大小
    if (offset + total_write > ip->disk_info.size)
        ip->disk_info.size = offset + total_write;

    // 写回inode元数据
    inode_rw(ip, true);

    return total_write;
}

static char* inode_type_list[] = { "DATA", "DIR", "DEVICE" };

/* 输出inode信息(for debug) */
void inode_print(inode_t* ip, char* name)
{
    assert(sleeplock_holding(&ip->slk), "inode_print: slk");

    spinlock_acquire(&lk_inode_cache);

    printf("inode %s:\n", name);
    printf("ref = %d, inode_num = %d, valid_info = %d\n", ip->ref, ip->inode_num, ip->valid_info);
    printf("type = %s, major = %d, minor = %d, nlink = %d, size = %d\n", inode_type_list[ip->disk_info.type],
        ip->disk_info.major, ip->disk_info.minor, ip->disk_info.nlink, ip->disk_info.size);

    printf("index_list = [ ");
    for (int i = 0; i < INODE_INDEX_1; i++)
        printf("%d ", ip->disk_info.index[i]);
    printf("] [ ");
    for (int i = INODE_INDEX_1; i < INODE_INDEX_2; i++)
        printf("%d ", ip->disk_info.index[i]);
    printf("] [ ");
    for (int i = INODE_INDEX_2; i < INODE_INDEX_3; i++)
        printf("%d ", ip->disk_info.index[i]);
    printf("]\n\n");

    spinlock_release(&lk_inode_cache);
}
