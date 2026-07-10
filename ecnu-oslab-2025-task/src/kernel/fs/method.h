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

/* fs.c: 文件系统 */

void fs_init();
