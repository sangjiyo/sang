#include "mod.h"

static buffer_node_t buf_cache[N_BUFFER];
static buffer_node_t buf_head_active, buf_head_inactive;
static spinlock_t lk_buf_cache;

/*
    将一个节点拿出来并插入
    1. 活跃链表的头部 buf_head_active->next
    2. 活跃链表的尾部 buf_head_active->prev
    3. 不活跃链表的头部 buf_head_inactive->next
    4. 不活跃链表的尾部 buf_head_inactive->prev
*/
static void insert_node(buffer_node_t *node, bool insert_active, bool insert_next)
{
    /* 如果有需要, 让node先离开当前位置 */
    if (node->next != NULL && node->prev != NULL) {
        node->next->prev = node->prev;
        node->prev->next = node->next;
    }

    /* 选择目标双向循环链表 */
    buffer_node_t *head = &buf_head_inactive;
    if (insert_active)
        head = &buf_head_active;

    /* 然后将node插入head->next or head->prev */
    if (insert_next) {
        node->next = head->next;
        node->next->prev = node;
        node->prev = head;
        head->next = node;
    } else {
        node->prev = head->prev;
        node->prev->next = node;
        node->next = head;
        head->prev = node;
    }
}

/*
    buffer系统初始化：
    1. 初始化全局的lk_buf_cache + buf_head_active + buf_head_inactive
    2. 初始化buf_cache中的所有node, 并将他们放在不活跃链表中
*/
void buffer_init()
{
    spinlock_init(&lk_buf_cache, "lk_buf_cache");

    // 初始化两个带头节点的双向循环链表
    buf_head_active.next = &buf_head_active;
    buf_head_active.prev = &buf_head_active;
    buf_head_inactive.next = &buf_head_inactive;
    buf_head_inactive.prev = &buf_head_inactive;

    // 初始化buf_cache中的所有node
    for (int i = 0; i < N_BUFFER; i++) {
        buffer_node_t *node = &buf_cache[i];
        buffer_t *buf = &node->buf;

        buf->block_num = BLOCK_NUM_UNUSED;
        buf->ref = 0;
        buf->data = NULL;
        buf->disk = false;
        sleeplock_init(&buf->slk, "buffer");

        // 插入不活跃链表 (head->next方向, 即第一个插入的在最前面)
        // 我们希望第一个buffer位于buf_head_inactive->next
        // 所以每个新节点都插入到head->next位置
        node->next = NULL;
        node->prev = NULL;
        insert_node(node, false, true);
    }
}

/* 磁盘读取: block -> buf */
static void buffer_read(buffer_t *buf)
{
    // 调用者必须持有睡眠锁
    if (!sleeplock_holding(&buf->slk))
        panic("buffer_read: not holding sleep lock");

    virtio_disk_rw(buf, false); // false = read
}

/* 磁盘写入: buf -> block */
void buffer_write(buffer_t *buf)
{
    // 调用者必须持有睡眠锁
    if (!sleeplock_holding(&buf->slk))
        panic("buffer_write: not holding sleep lock");

    virtio_disk_rw(buf, true); // true = write
}

/* 从buf_cache中获取一个buf */
buffer_t* buffer_get(uint32 block_num)
{
    buffer_node_t *node;
    buffer_t *buf;

    spinlock_acquire(&lk_buf_cache);

    // 1. 先在活跃链表中寻找 (从head->next开始, 即LRU最活跃端)
    for (node = buf_head_active.next; node != &buf_head_active; node = node->next) {
        if (node->buf.block_num == block_num) {
            // 找到! 移动到活跃链表头部 (head->next)
            insert_node(node, true, true);
            buf = &node->buf;
            buf->ref++;
            spinlock_release(&lk_buf_cache);
            // 上睡眠锁后返回
            sleeplock_acquire(&buf->slk);
            return buf;
        }
    }

    // 2. 在不活跃链表中寻找 (从head->next开始)
    for (node = buf_head_inactive.next; node != &buf_head_inactive; node = node->next) {
        if (node->buf.block_num == block_num) {
            // 找到! 如果data为空则申请物理页
            if (node->buf.data == NULL) {
                node->buf.data = (uint8*)pmem_alloc(true);
                memset(node->buf.data, 0, BLOCK_SIZE);
            }
            // 移动到活跃链表头部
            insert_node(node, true, true);
            buf = &node->buf;
            buf->ref++;
            spinlock_release(&lk_buf_cache);
            sleeplock_acquire(&buf->slk);
            return buf;
        }
    }

    // 3. 缓存未命中: 取不活跃链表尾部 (最不活跃的元素)
    node = buf_head_inactive.prev;
    if (node == &buf_head_inactive)
        panic("buffer_get: no available buffer");

    // 如果data为空则申请物理页
    if (node->buf.data == NULL) {
        node->buf.data = (uint8*)pmem_alloc(true);
        memset(node->buf.data, 0, BLOCK_SIZE);
    }

    // 设置新的block_num
    node->buf.block_num = block_num;

    // 移动到活跃链表头部
    insert_node(node, true, true);
    buf = &node->buf;
    buf->ref++;
    spinlock_release(&lk_buf_cache);

    // 上睡眠锁后从磁盘读入目标block
    sleeplock_acquire(&buf->slk);
    buffer_read(buf);

    return buf;
}

/* 向buf_cache归还一个buf */
void buffer_put(buffer_t *buf)
{
    // buf是buffer_node_t的第一个字段, 地址相同
    buffer_node_t *node = (buffer_node_t*)buf;

    spinlock_acquire(&lk_buf_cache);

    // 释放睡眠锁
    if (sleeplock_holding(&buf->slk))
        sleeplock_release(&buf->slk);

    // 引用计数减1
    if (buf->ref > 0)
        buf->ref--;

    // 如果引用计数归零, 移动到不活跃链表头部
    if (buf->ref == 0) {
        insert_node(node, false, true);
    }

    spinlock_release(&lk_buf_cache);
}

/*
    从后向前遍历非活跃链表, 尝试释放buffer_count个buffer持有的物理内存(data)
    返回成功释放资源的buffer数量
*/
uint32 buffer_freemem(uint32 buffer_count)
{
    buffer_node_t *node;
    uint32 freed = 0;

    spinlock_acquire(&lk_buf_cache);

    // 从尾部 (最不活跃) 向前遍历
    for (node = buf_head_inactive.prev;
         node != &buf_head_inactive && freed < buffer_count;
         /* 在循环体内手动推进 */) {

        buffer_node_t *prev = node->prev;

        if (node->buf.data != NULL) {
            pmem_free((uint64)node->buf.data, true);
            node->buf.data = NULL;
            node->buf.block_num = BLOCK_NUM_UNUSED;
            freed++;
        }

        node = prev;
    }

    spinlock_release(&lk_buf_cache);
    return freed;
}

/* 输出buffer_cache的信息 (for test) */
void buffer_print_info()
{
    buffer_node_t *node;

    assert(N_BUFFER == N_BUFFER_TEST, "buffer_print_info: invalid N_BUFFER");

    spinlock_acquire(&lk_buf_cache);

    printf("buffer_cache information:\n");

    printf("1.active list:\n");
    for (node = buf_head_active.next; node != &buf_head_active; node = node->next) {
        printf("buffer %d(ref = %d): page(pa = %x) -> block[%d]\n",
            (int)(node - buf_cache), node->buf.ref, (uint64)node->buf.data, node->buf.block_num);
    }
    printf("over!\n");

    printf("2.inactive list:\n");
    for (node = buf_head_inactive.next; node != &buf_head_inactive; node = node->next) {
        printf("buffer %d(ref = %d): page(pa = %x) -> block[%d]\n",
            (int)(node - buf_cache), node->buf.ref, (uint64)node->buf.data, node->buf.block_num);
    }
    printf("over!\n");

    spinlock_release(&lk_buf_cache);
}
