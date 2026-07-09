#include "mod.h"

// mmap_region_node_t 仓库(单向链表) + 链表头节点(不可分配) + 保护仓库的自旋锁
static mmap_region_node_t node_list[N_MMAP];
static mmap_region_node_t list_head;
static spinlock_t list_lk;

// 初始化上述三个数据结构
void mmap_init()
{
    spinlock_init(&list_lk, "mmap_list");
    list_head.next = NULL;
    for (int i = 0; i < N_MMAP; i++) {
        node_list[i].next = list_head.next;
        list_head.next = &node_list[i];
    }
}

// 从仓库申请一个 mmap_region_t
// 若仓库空了则 panic
mmap_region_t *mmap_region_alloc()
{
    spinlock_acquire(&list_lk);
    if (list_head.next == NULL){
        spinlock_release(&list_lk);
        panic("mmap_region_alloc: no free node");
    }
    mmap_region_node_t* node = list_head.next;
    list_head.next = node->next;
    spinlock_release(&list_lk);
    memset(&node->mmap, 0, sizeof(mmap_region_t));
    node->next = NULL; // 关键：清除 next
    return &node->mmap;
}

// 向仓库归还一个 mmap_region_t
void mmap_region_free(mmap_region_t *mmap)
{
    spinlock_acquire(&list_lk);
    mmap_region_node_t* node = (mmap_region_node_t*)mmap;
    node->next = list_head.next;
    list_head.next = node;
    spinlock_release(&list_lk);
}

// 输出可用的 mmap_region_node_t 链
// for debug
void mmap_show_nodelist()
{
    spinlock_acquire(&list_lk);
    mmap_region_node_t *tmp = list_head.next;
    int node = 0, index = 0;
    while (tmp)
    {
        index = tmp - &(node_list[0]);
        printf("node %d index = %d\n", node++, index);
        tmp = tmp->next;
    }
    spinlock_release(&list_lk);
}