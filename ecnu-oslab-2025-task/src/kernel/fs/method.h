#pragma once

/* virtio.c: 以block为单位的磁盘读写能力 */

void virtio_disk_init();
void virtio_disk_rw(buffer_t *b, bool write);
void virtio_disk_intr();

/* buffer.c: 以buffer为中介沟通内存和磁盘 */

void buffer_init();
buffer_t* buffer_get(uint32 block_num);
void buffer_put(buffer_t *buf);
void buffer_write(buffer_t *buf);
uint32 buffer_freemem(uint32 buffer_count);
void buffer_print_info();

/* bitmap.c: data_bitmap和inode_bitmap的管理 */

uint32 bitmap_alloc_block();
uint32 bitmap_alloc_inode();
void bitmap_free_block(uint32 block_num);
void bitmap_free_inode(uint32 inode_num);
void bitmap_print(bool print_data_bitmap);

/* inode.c: 索引节点的管理 */

void inode_init();
void inode_rw(inode_t *ip, bool write);
inode_t *inode_get(uint32 inode_num);
inode_t *inode_create(uint16 type, uint16 major, uint16 minor);
inode_t* inode_dup(inode_t* ip);
void inode_lock(inode_t* ip);
void inode_unlock(inode_t *ip);
void inode_put(inode_t* ip);
void inode_delete(inode_t *ip);
uint32 inode_read_data(inode_t *ip, uint32 offset, uint32 len, void *dst, bool is_user_dst);
uint32 inode_write_data(inode_t *ip, uint32 offset, uint32 len, void *src, bool is_user_src);
void inode_print(inode_t *ip, char* name);

/* dentry.c: 关于目录项和文件路径 */

uint32 dentry_search(inode_t *ip, char *name);
uint32 dentry_create(inode_t *ip, uint32 inode_num, char *name);
uint32 dentry_delete(inode_t *ip, char *name);
void dentry_print(inode_t *ip);
inode_t* path_to_inode(char *path);
inode_t* path_to_parent_inode(char *path, char *name);

/* fs.c: 文件系统 */

void fs_init();
