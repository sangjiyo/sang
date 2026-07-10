#include "mod.h"

// 内核空间和用户空间的可分配物理页分开描述
static alloc_region_t kern_region, user_region;

// 物理内存的初始化
// 本质上就是填写kern_region和user_region, 包括基本数值和空闲链表
void pmem_init(void)
{
    // 初始化两个区域的自旋锁
    spinlock_init(&kern_region.lk, "kern_region");
    spinlock_init(&user_region.lk, "user_region");

    // 设置区域范围
    kern_region.begin = (uint64)ALLOC_BEGIN;
    kern_region.end = (uint64)ALLOC_BEGIN + KERN_PAGES * PGSIZE;
    user_region.begin = kern_region.end;
    user_region.end = (uint64)ALLOC_END;

    // 初始化链表头
    kern_region.list_head.next = NULL;
    user_region.list_head.next = NULL;

    // 计算可用页面数
    kern_region.allocable = (kern_region.end - kern_region.begin) / PGSIZE;
    user_region.allocable = (user_region.end - user_region.begin) / PGSIZE;

    // 构建空闲链表（将每个物理页作为节点插入）
    // 注意：使用头插法，顺序不重要
    page_node_t* node;
    uint64 pa;

    // 内核区
    for (pa = kern_region.begin; pa < kern_region.end; pa += PGSIZE) {
        node = (page_node_t*)pa;
        node->next = kern_region.list_head.next;
        kern_region.list_head.next = node;
    }

    // 用户区
    for (pa = user_region.begin; pa < user_region.end; pa += PGSIZE) {
        node = (page_node_t*)pa;
        node->next = user_region.list_head.next;
        user_region.list_head.next = node;
    }
}

// 尝试返回一个可分配的清零后的物理页
// 失败则panic锁死
void* pmem_alloc(bool in_kernel)
{

    alloc_region_t* region = in_kernel ? &kern_region : &user_region;
    page_node_t *page;

    spinlock_acquire(&region->lk);

    // 从链表头摘取第一个空闲页
    page = region->list_head.next;
    if (page == NULL) {
        spinlock_release(&region->lk);
        panic("pmem_alloc: out of memory");
    }

    // 更新链表头
    region->list_head.next = page->next;
    region->allocable--;

    spinlock_release(&region->lk);

    // 清零该页（因为分配出去后可能存放敏感数据）
    memset(page, 0, PGSIZE);


    return page;
}

// 释放一个物理页
// 失败则panic锁死
void pmem_free(uint64 page, bool in_kernel)
{

    alloc_region_t* region = in_kernel ? &kern_region : &user_region;

    // 检查页对齐
    if (page % PGSIZE != 0)
        panic("pmem_free: not page aligned");

    // 检查页是否属于该区域
    if (page < region->begin || page >= region->end)
        panic("pmem_free: page out of region");

    spinlock_acquire(&region->lk);

    // 将释放的页插入链表头部
    page_node_t* node = (page_node_t*)page;
    node->next = region->list_head.next;
    region->list_head.next = node;
    region->allocable++;

    spinlock_release(&region->lk);


}
