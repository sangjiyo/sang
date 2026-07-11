#include "mod.h"

/*
    出于简化目的的假设:
    如果inode_disk.type == INODE_TYPE_DIR
    那么inode_disk.size <= BLOCKSIZE (只有inode_disk.index[0]有效)
    也就是说, 单个目录最多包含BLOCKSIZE / sizeof(dentry)个目录项

    另外, INODE_TYPE_DATA要求数据之间没有空隙
    但是对于INODE_TYPE_DIR来说是无法做到的(目录项的删除很常见)
    因此, ip->size代表block中已经使用的空间大小
*/


/*----------------dentry的查找、增加、删除操作-----------------*/

/*
    在目录ip中查找是否存在名字为name的目录项
    如果找到了返回目录项中存储的inode_num
    如果没找到返回INVALID_INODE_NUM
    注意: 调用者需要持有ip->slk
*/
uint32 dentry_search(inode_t* ip, char* name)
{
    // 确保index[0]有效
    if (ip->disk_info.index[0] == 0)
        return INVALID_INODE_NUM;

    buffer_t* buf = buffer_get(ip->disk_info.index[0]);
    dentry_t* de = (dentry_t*)(buf->data);
    uint32 count = ip->disk_info.size / sizeof(dentry_t);

    for (uint32 i = 0; i < count; i++) {
        if (de[i].name[0] != '\0' &&
            strncmp(de[i].name, name, MAXLEN_FILENAME) == 0) {
            uint32 inode_num = de[i].inode_num;
            buffer_put(buf);
            return inode_num;
        }
    }

    buffer_put(buf);
    return INVALID_INODE_NUM;
}

/*
    在目录ip中寻找空闲槽位, 插入新的dentry
    如果成功插入则返回这个目录项的偏移量(还需要更新size)
    如果插入失败(没有空间/发生重名)返回-1
    注意: 调用者需要持有ip->slk
*/
uint32 dentry_create(inode_t* ip, uint32 inode_num, char* name)
{
    // 检查重名
    if (dentry_search(ip, name) != INVALID_INODE_NUM)
        return -1; // 重名

    // 确保index[0]存在
    if (ip->disk_info.index[0] == 0) {
        ip->disk_info.index[0] = bitmap_alloc_block();
        // 初始化新分配的数据块
        buffer_t* init_buf = buffer_get(ip->disk_info.index[0]);
        memset(init_buf->data, 0, BLOCK_SIZE);
        buffer_write(init_buf);
        buffer_put(init_buf);
        ip->disk_info.size = 0;
    }

    buffer_t* buf = buffer_get(ip->disk_info.index[0]);
    dentry_t* de = (dentry_t*)(buf->data);
    uint32 max_dentries = BLOCK_SIZE / sizeof(dentry_t);
    uint32 count = ip->disk_info.size / sizeof(dentry_t);

    // 首先在已有范围内查找空闲槽位(已删除的目录项)
    for (uint32 i = 0; i < count; i++) {
        if (de[i].name[0] == '\0') {
            // 找到空闲槽位
            de[i].inode_num = inode_num;
            memmove(de[i].name, name, MAXLEN_FILENAME - 1);
            de[i].name[MAXLEN_FILENAME - 1] = '\0';
            buffer_write(buf);
            uint32 offset = i * sizeof(dentry_t);
            buffer_put(buf);
            return offset;
        }
    }

    // 没有空闲槽位, 尝试在末尾新增
    if (count >= max_dentries) {
        buffer_put(buf);
        return -1; // 目录已满
    }

    // 在末尾插入
    de[count].inode_num = inode_num;
    memmove(de[count].name, name, MAXLEN_FILENAME - 1);
    de[count].name[MAXLEN_FILENAME - 1] = '\0';
    buffer_write(buf);

    // 更新size
    uint32 offset = count * sizeof(dentry_t);
    ip->disk_info.size = (count + 1) * sizeof(dentry_t);

    buffer_put(buf);
    return offset;
}

/*
    在目录ip下删除名称为name的dentry, 返回它的inode_num
    如果匹配失败或者遇到非法情况返回INVALID_INODE_NUM
    注意: 调用者需要持有ip->slk
*/
uint32 dentry_delete(inode_t* ip, char* name)
{
    if (ip->disk_info.index[0] == 0)
        return INVALID_INODE_NUM;

    buffer_t* buf = buffer_get(ip->disk_info.index[0]);
    dentry_t* de = (dentry_t*)(buf->data);
    uint32 count = ip->disk_info.size / sizeof(dentry_t);

    for (uint32 i = 0; i < count; i++) {
        if (de[i].name[0] != '\0' &&
            strncmp(de[i].name, name, MAXLEN_FILENAME) == 0) {
            // 找到, 标记为空闲
            uint32 inode_num = de[i].inode_num;
            de[i].name[0] = '\0';
            de[i].inode_num = INVALID_INODE_NUM;
            buffer_write(buf);
            buffer_put(buf);
            return inode_num;
        }
    }

    buffer_put(buf);
    return INVALID_INODE_NUM;
}

/* 输出目录中所有有效目录项的信息 (for debug) */
void dentry_print(inode_t* ip)
{
    assert(sleeplock_holding(&ip->slk), "dentry_print: slk!");
    assert(ip->disk_info.type == INODE_TYPE_DIR, "dentry_print: not dir!");

    dentry_t* de;
    buffer_t* buf;

    if (ip->disk_info.index[0] == 0)
        panic("dentry_print: invalid index[0]!");

    printf("inode_num = %d, dentries:\n", ip->inode_num);

    buf = buffer_get(ip->disk_info.index[0]);
    for (de = (dentry_t*)(buf->data); de < (dentry_t*)(buf->data + BLOCK_SIZE); de++)
    {
        if (de->name[0] != 0) {
            printf("dentry: offset = %d, inode_num = %d, name = %s\n",
                (uint32)((uint8*)de - buf->data), de->inode_num, de->name);
        }
    }
    buffer_put(buf);

    printf("\n");
}

/*------------------从文件名到文件路径-----------------*/

/*
    Examples:
    get_element("a/bb/c", name) = "bb/c" + name = "a"
    get_element("///aa//bb", name) = "bb" + name = "aa"
    get_element("aaa", name) = "" + name = "aaa"
    get_element("", name) = NULL + name = ""
    get_element("//", name) = NULL + name = ""
*/
static char* get_element(char* path, char* name)
{
    /* 跳过前置的'/' */
    while (*path == '/')
        path++;

    /* 如果遇到末尾了则返回 */
    if (*path == 0) {
        name[0] = 0;
        return NULL;
    }

    /* 记录起点位置 */
    char* start = path;

    /* 推进path直到遇到'/'或者到达末尾 */
    while (*path != '/' && *path != 0)
        path++;

    /* 提取到的name的长度 */
    int len = path - start;
    len = MIN(len, MAXLEN_FILENAME - 1);

    /* 设置name */
    memmove(name, start, len);
    name[len] = 0;

    /* 跳过后置的'/' */
    while (*path == '/') path++;

    return path;
}
/*
    根据文件路径(/A/B/C)查找对应inode(inode_B or inode_C)
    如果find_parent_inode == true, 返回父节点inode, name为下一级子节点的名字
    如果find_parent_inode == false, 返回子节点inode, name无意义
    如果失败返回NULL
*/
static inode_t* __path_to_inode(char* path, char* name, bool find_parent_inode)
{
    inode_t* ip, * next;
    char element_name[MAXLEN_FILENAME];

    // 从根节点开始
    ip = inode_get(ROOT_INODE);
    inode_lock(ip);

    // 逐级解析path
    while (1) {
        path = get_element(path, element_name);

        // 路径结束
        if (path == NULL || element_name[0] == '\0') {
            if (find_parent_inode) {
                name[0] = '\0';
            }
            // 返回前解锁, 调用者按需通过inode_lock获取
            inode_unlock(ip);
            return ip;
        }

        // 还未到达目标: 在当前目录中查找下一级
        uint32 next_inode_num = dentry_search(ip, element_name);
        if (next_inode_num == INVALID_INODE_NUM) {
            inode_unlock(ip);
            inode_put(ip);
            return NULL;
        }

        // 检查是否到达目标
        if (path == NULL || *path == '\0') {
            // path已经结束, element_name是最后一个元素
            if (find_parent_inode) {
                // 返回当前ip作为父节点, element_name作为name
                memmove(name, element_name, MAXLEN_FILENAME);
                inode_unlock(ip);
                return ip;
            }
        }

        // 获取下一级inode
        next = inode_get(next_inode_num);
        inode_unlock(ip);
        inode_put(ip);
        ip = next;
        inode_lock(ip);
    }
}

/*
    基于path寻找inode
    失败返回NULL
*/
inode_t* path_to_inode(char* path)
{
    char name[MAXLEN_FILENAME];
    return __path_to_inode(path, name, false);
}

/*
    基于path寻找inode->parent, 将inode->name放入name
    失败返回NULL, 同时name无效
*/
inode_t* path_to_parent_inode(char* path, char* name)
{
    return __path_to_inode(path, name, true);
}
