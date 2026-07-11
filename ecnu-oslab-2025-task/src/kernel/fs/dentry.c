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
    在目录ip中查找是否存在序号为inode_num的目录项
    如果存在则将它的名字拷贝到name, 返回name_len
    如果不存在则返回-1
    注意: 调用者需要持有ip->slk
*/
uint32 dentry_search_2(inode_t* ip, uint32 inode_num, char* name)
{
    if (ip->disk_info.index[0] == 0)
        return -1;

    buffer_t* buf = buffer_get(ip->disk_info.index[0]);
    dentry_t* de = (dentry_t*)(buf->data);
    uint32 count = ip->disk_info.size / sizeof(dentry_t);

    for (uint32 i = 0; i < count; i++) {
        if (de[i].name[0] != '\0' && de[i].inode_num == inode_num) {
            uint32 name_len = strlen(de[i].name);
            memmove(name, de[i].name, name_len);
            name[name_len] = '\0';
            buffer_put(buf);
            return name_len;
        }
    }

    buffer_put(buf);
    return -1;
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

/*
    向缓冲区[dst, dst + len)中填充有效的dentry
    返回成功填充的数据量(字节)
    注意: 调用者需持有ip->slk
*/
uint32 dentry_transmit(inode_t* ip, uint64 dst, uint32 len, bool is_user_dst)
{
    if (ip->disk_info.index[0] == 0)
        return 0;

    buffer_t* buf = buffer_get(ip->disk_info.index[0]);
    dentry_t* de_list = (dentry_t*)(buf->data);
    uint32 count = ip->disk_info.size / sizeof(dentry_t);

    uint32 total = 0;
    proc_t* p = myproc();

    for (uint32 i = 0; i < count && total + sizeof(dentry_t) <= len; i++) {
        if (de_list[i].name[0] != '\0') {
            if (is_user_dst)
                uvm_copyout(p->pgtbl, dst + total, (uint64)&de_list[i], sizeof(dentry_t));
            else
                memmove((void*)(dst + total), &de_list[i], sizeof(dentry_t));
            total += sizeof(dentry_t);
        }
    }

    buffer_put(buf);
    return total;
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

    // 支持相对路径: 不以'/'开头则从cwd开始
    if (path[0] != '/') {
        ip = myproc()->cwd;
        if (ip == NULL)
            ip = inode_get(ROOT_INODE);
        else
            ip = inode_dup(ip);
        inode_lock(ip);
    }
    else {
        // 从根节点开始
        ip = inode_get(ROOT_INODE);
        inode_lock(ip);
    }

    // 逐级解析path
    while (1) {
        path = get_element(path, element_name);

        // 路径结束 (根目录或空路径)
        if (path == NULL || element_name[0] == '\0') {
            if (find_parent_inode)
                name[0] = '\0';
            inode_unlock(ip);
            return ip;
        }

        // element_name是最后一个元素吗?
        bool is_last = (path == NULL || *path == '\0');

        // 在当前目录中查找element_name
        uint32 next_inode_num = dentry_search(ip, element_name);

        if (next_inode_num == INVALID_INODE_NUM) {
            // 找不到element_name
            if (is_last && find_parent_inode) {
                // 最后一个元素不存在 → 当前ip就是父节点, 返回它
                // (用于path_create_inode: 父目录存在, 子项待创建)
                memmove(name, element_name, MAXLEN_FILENAME);
                inode_unlock(ip);
                return ip;
            }
            // 中间元素缺失 → 失败
            inode_unlock(ip);
            inode_put(ip);
            return NULL;
        }

        // 找到了element_name
        if (is_last) {
            // 最后一个元素
            if (find_parent_inode) {
                // 返回当前ip作为父节点, element_name作为name
                memmove(name, element_name, MAXLEN_FILENAME);
                inode_unlock(ip);
                return ip;
            }
            // 返回目标inode
            next = inode_get(next_inode_num);
            inode_unlock(ip);
            inode_put(ip);
            inode_lock(next);
            inode_unlock(next);
            return next;
        }

        // 中间元素: 继续向下
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

/*
    将inode对应的完整路径填入path中(缓冲区长度为len)
    成功返回偏移量(从path+offset开始有效), 失败返回-1
*/
uint32 inode_to_path(inode_t* ip, char* path, uint32 len)
{
    if (len == 0) return -1;

    // 从后往前填充缓冲区: offset指向未使用位置
    uint32 offset = len;
    path[--offset] = '\0';

    inode_t* cur = ip;
    inode_dup(cur);
    inode_lock(cur);

    while (1) {
        // 到达根节点: 结束
        if (cur->inode_num == ROOT_INODE) {
            if (offset == len - 1) {
                // 根节点自身: 至少输出"/"
                if (offset == 0) { inode_unlock(cur); inode_put(cur); return -1; }
                path[--offset] = '/';
            }
            break;
        }

        // 通过".."查找父节点inode_num
        if (cur->disk_info.index[0] == 0) {
            inode_unlock(cur); inode_put(cur); return -1;
        }

        uint32 cur_num = cur->inode_num;
        uint32 parent_num = INVALID_INODE_NUM;

        buffer_t* buf = buffer_get(cur->disk_info.index[0]);
        dentry_t* de = (dentry_t*)(buf->data);
        uint32 count = cur->disk_info.size / sizeof(dentry_t);
        for (uint32 i = 0; i < count; i++) {
            if (de[i].name[0] != '\0' &&
                strncmp(de[i].name, "..", MAXLEN_FILENAME) == 0) {
                parent_num = de[i].inode_num;
                break;
            }
        }
        buffer_put(buf);

        if (parent_num == INVALID_INODE_NUM) {
            inode_unlock(cur); inode_put(cur); return -1;
        }

        // 切换到父节点
        inode_unlock(cur);
        inode_put(cur);
        cur = inode_get(parent_num);
        inode_lock(cur);

        // 在父节点中查找cur_num的name
        if (cur->disk_info.index[0] == 0) {
            inode_unlock(cur); inode_put(cur); return -1;
        }

        buf = buffer_get(cur->disk_info.index[0]);
        de = (dentry_t*)(buf->data);
        count = cur->disk_info.size / sizeof(dentry_t);

        bool found = false;
        for (uint32 i = 0; i < count; i++) {
            if (de[i].name[0] != '\0' && de[i].inode_num == cur_num) {
                uint32 name_len = strlen(de[i].name);
                if (offset < name_len + 1) {
                    buffer_put(buf);
                    inode_unlock(cur); inode_put(cur); return -1;
                }
                offset -= name_len;
                memmove(path + offset, de[i].name, name_len);
                // 如果上面还有路径, 加'/'
                if (cur->inode_num != ROOT_INODE && offset > 0)
                    path[--offset] = '/';
                found = true;
                break;
            }
        }
        buffer_put(buf);

        if (!found) {
            inode_unlock(cur); inode_put(cur); return -1;
        }
    }

    inode_unlock(cur);
    inode_put(cur);
    return offset;
}

/*
    基于path创建新的inode
    成功返回inode, 失败返回NULL
*/
inode_t* path_create_inode(char* path, uint16 type, uint16 major, uint16 minor)
{
    char name[MAXLEN_FILENAME];
    inode_t* parent = path_to_parent_inode(path, name);

    if (parent == NULL || name[0] == '\0')
        return NULL;

    inode_lock(parent);

    // 检查父节点是否为目录
    if (parent->disk_info.type != INODE_TYPE_DIR) {
        inode_unlock(parent);
        inode_put(parent);
        return NULL;
    }

    // 创建新的inode
    inode_t* ip = inode_create(type, major, minor);
    if (ip == NULL) {
        inode_unlock(parent);
        inode_put(parent);
        return NULL;
    }

    // 在父目录中创建目录项
    inode_lock(ip);

    // 如果是目录类型, 初始化数据块并设置 "." 和 ".."
    if (type == INODE_TYPE_DIR) {
        if (ip->disk_info.index[0] == 0) {
            ip->disk_info.index[0] = bitmap_alloc_block();
            buffer_t* dir_buf = buffer_get(ip->disk_info.index[0]);
            memset(dir_buf->data, 0, BLOCK_SIZE);
            dentry_t* de = (dentry_t*)(dir_buf->data);
            de[0].inode_num = ip->inode_num;
            de[1].inode_num = parent->inode_num;
            memmove(de[0].name, ".", 2);
            memmove(de[1].name, "..", 3);
            ip->disk_info.size = 2 * sizeof(dentry_t);
            buffer_write(dir_buf);
            buffer_put(dir_buf);
            inode_rw(ip, true);
        }
    }

    if (dentry_create(parent, ip->inode_num, name) == (uint32)-1) {
        inode_unlock(ip);
        inode_put(ip);
        inode_unlock(parent);
        inode_put(parent);
        return NULL;
    }

    // 写回父目录
    inode_rw(parent, true);

    inode_unlock(ip);
    inode_unlock(parent);
    inode_put(parent);

    return ip;
}

/*
    构建文件硬链接 (new_path 指向 old_path 指向的 inode)
    核心操作包括 nlink++ 和 dentry_create()
    注意: old_path指向的inode不能是目录类型的
    成功返回0, 失败返回-1
*/
uint32 path_link(char* old_path, char* new_path)
{
    // 获取旧路径的inode
    inode_t* ip = path_to_inode(old_path);
    if (ip == NULL)
        return -1;

    inode_lock(ip);

    // 目录不能硬链接
    if (ip->disk_info.type == INODE_TYPE_DIR) {
        inode_unlock(ip);
        inode_put(ip);
        return -1;
    }

    // 获取新路径的父目录和名字
    char name[MAXLEN_FILENAME];
    inode_t* parent = path_to_parent_inode(new_path, name);
    if (parent == NULL || name[0] == '\0') {
        inode_unlock(ip);
        inode_put(ip);
        return -1;
    }

    inode_lock(parent);

    // 在父目录中创建dentry
    if (dentry_create(parent, ip->inode_num, name) == (uint32)-1) {
        inode_unlock(parent);
        inode_put(parent);
        inode_unlock(ip);
        inode_put(ip);
        return -1;
    }

    // nlink++
    ip->disk_info.nlink++;
    inode_rw(ip, true);
    inode_rw(parent, true);

    inode_unlock(parent);
    inode_put(parent);
    inode_unlock(ip);
    inode_put(ip);

    return 0;
}

/*
    解除文件硬链接
    成功返回0, 失败返回-1
*/
uint32 path_unlink(char* path)
{
    char name[MAXLEN_FILENAME];
    inode_t* parent = path_to_parent_inode(path, name);

    if (parent == NULL || name[0] == '\0')
        return -1;

    inode_lock(parent);

    // 在父目录中删除dentry
    uint32 inode_num = dentry_delete(parent, name);
    if (inode_num == INVALID_INODE_NUM) {
        inode_unlock(parent);
        inode_put(parent);
        return -1;
    }

    // 写回父目录
    inode_rw(parent, true);

    // 获取被删除的inode, nlink--
    inode_t* ip = inode_get(inode_num);
    inode_lock(ip);
    if (ip->disk_info.nlink > 0)
        ip->disk_info.nlink--;
    inode_rw(ip, true);
    inode_unlock(ip);
    inode_put(ip);  // nlink可能归零, inode_put会触发inode_delete

    inode_unlock(parent);
    inode_put(parent);

    return 0;
}