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
uint32 dentry_search_2(inode_t *ip, uint32 inode_num, char *name);
uint32 dentry_create(inode_t *ip, uint32 inode_num, char *name);
uint32 dentry_delete(inode_t *ip, char *name);
uint32 dentry_transmit(inode_t *ip, uint64 dst, uint32 len, bool is_user_dst);
void dentry_print(inode_t *ip);
inode_t* path_to_inode(char *path);
inode_t* path_to_parent_inode(char *path, char *name);
uint32 inode_to_path(inode_t *ip, char *path, uint32 len);
inode_t* path_create_inode(char *path, uint16 type, uint16 major, uint16 minor);
uint32 path_link(char *old_path, char *new_path);
uint32 path_unlink(char *path);

/* fs.c: 文件系统 */

void file_init();
file_t* file_alloc();
file_t* file_open(char *path, uint32 open_mode);
void file_close(file_t *file);
uint32 file_read(file_t* file, uint32 len, uint64 dst, bool is_user_dst);
uint32 file_write(file_t* file, uint32 len, uint64 src, bool is_user_src);
uint32 file_lseek(file_t *file, uint32 lseek_offset, uint32 lseek_flag);
file_t* file_dup(file_t* file);
uint32 file_get_stat(file_t* file, uint64 user_dst);
void fs_init();

/* device.c: 设备文件 */

void device_init();
bool device_open_check(uint16 major, uint32 open_mode);
uint32 device_read_data(uint16 major, uint32 len, uint64 dst, bool is_user_dst);
uint32 device_write_data(uint16 major, uint32 len, uint64 src, bool is_user_src);
