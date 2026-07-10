#include "mod.h"

extern super_block_t sb;

/*
    查询一个block中的所有bit, 找到空闲bit, 设置1并返回
    如果没有空闲bit, 返回-1
*/
static uint32 bitmap_search_and_set(uint32 bitmap_block_num, uint32 valid_count)
{
    buffer_t *buf = buffer_get(bitmap_block_num);

    // 逐字节遍历
    for (uint32 byte = 0; byte < BLOCK_SIZE; byte++) {
        uint8 mask = buf->data[byte];

        // 如果这个字节不是全1, 说明有空闲bit
        if (mask != 0xFF) {
            // 逐bit寻找第一个0
            for (uint32 shift = 0; shift < BIT_PER_BYTE; shift++) {
                uint32 bit_index = byte * BIT_PER_BYTE + shift;

                // 检查是否在有效范围内
                if (bit_index >= valid_count) {
                    buffer_put(buf);
                    return -1; // 超过有效范围, 没有空闲bit
                }

                if (!(mask & (1U << shift))) {
                    // 找到空闲bit! 设置为1
                    buf->data[byte] |= (uint8)(1U << shift);
                    buffer_write(buf);
                    buffer_put(buf);
                    return bit_index;
                }
            }
        }
    }

    buffer_put(buf);
    return -1; // 没有空闲bit
}

/*
    将block中第index个bit设为0
*/
static void bitmap_clear(uint32 bitmap_block_num, uint32 index)
{
    buffer_t *buf = buffer_get(bitmap_block_num);

    uint32 byte = index / BIT_PER_BYTE;
    uint32 shift = index % BIT_PER_BYTE;

    buf->data[byte] &= ~((uint8)(1U << shift));
    buffer_write(buf);
    buffer_put(buf);
}

/*
    获取一个空闲block, 将data_bitmap对应bit设为1
    返回这个block的全局序号
*/
uint32 bitmap_alloc_block()
{
    uint32 first_block = sb.data_bitmap_firstblock;
    uint32 bitmap_blocks = sb.data_bitmap_blocks;
    uint32 total_bits = sb.data_blocks;
    uint32 current_bit = 0;

    for (uint32 block = 0; block < bitmap_blocks; block++) {
        uint32 bitmap_block_num = first_block + block;
        uint32 bits_in_this_block = BIT_PER_BLOCK;

        // 最后一个block可能不满
        if (current_bit + BIT_PER_BLOCK > total_bits)
            bits_in_this_block = total_bits - current_bit;

        int32 found = bitmap_search_and_set(bitmap_block_num, bits_in_this_block);
        if (found >= 0) {
            // 返回全局block序号
            return sb.data_firstblock + current_bit + found;
        }

        current_bit += BIT_PER_BLOCK;
    }

    panic("bitmap_alloc_block: no free block");
    return -1;
}

/*
    获取一个空闲inode, 将inode_bitmap对应bit设为1
    返回这个inode的全局序号
*/
uint32 bitmap_alloc_inode()
{
    uint32 first_block = sb.inode_bitmap_firstblock;
    uint32 bitmap_blocks = sb.inode_bitmap_blocks;
    uint32 total_bits = sb.total_inodes;
    uint32 current_bit = 0;

    for (uint32 block = 0; block < bitmap_blocks; block++) {
        uint32 bitmap_block_num = first_block + block;
        uint32 bits_in_this_block = BIT_PER_BLOCK;

        // 最后一个block可能不满
        if (current_bit + BIT_PER_BLOCK > total_bits)
            bits_in_this_block = total_bits - current_bit;

        int32 found = bitmap_search_and_set(bitmap_block_num, bits_in_this_block);
        if (found >= 0) {
            // 返回全局inode序号
            return current_bit + found;
        }

        current_bit += BIT_PER_BLOCK;
    }

    panic("bitmap_alloc_inode: no free inode");
    return -1;
}

/* 释放一个block, 将data_bitmap对应bit设为0 */
void bitmap_free_block(uint32 block_num)
{
    uint32 first_block = sb.data_bitmap_firstblock;
    uint32 global_base = sb.data_firstblock;
    uint32 total_bits = sb.data_blocks;

    // 计算在bitmap中的偏移
    if (block_num < global_base || block_num >= global_base + total_bits)
        panic("bitmap_free_block: invalid block_num");

    uint32 bit_index = block_num - global_base;
    uint32 bitmap_block_offset = bit_index / BIT_PER_BLOCK;
    uint32 bit_in_block = bit_index % BIT_PER_BLOCK;

    uint32 bitmap_block_num = first_block + bitmap_block_offset;
    bitmap_clear(bitmap_block_num, bit_in_block);
}

/* 释放一个inode, 将inode_bitmap对应bit设为0 */
void bitmap_free_inode(uint32 inode_num)
{
    uint32 first_block = sb.inode_bitmap_firstblock;
    uint32 total_bits = sb.total_inodes;

    // 计算在bitmap中的偏移
    if (inode_num >= total_bits)
        panic("bitmap_free_inode: invalid inode_num");

    uint32 bitmap_block_offset = inode_num / BIT_PER_BLOCK;
    uint32 bit_in_block = inode_num % BIT_PER_BLOCK;

    uint32 bitmap_block_num = first_block + bitmap_block_offset;
    bitmap_clear(bitmap_block_num, bit_in_block);
}

/* 打印某个bitmap中所有分配出去的bit */
void bitmap_print(bool print_data_bitmap)
{
    uint32 first_block, bitmap_blocks, total_bits;
    uint32 global_base, current_bit = 0;

    if (print_data_bitmap) {
        printf("data bitmap alloced bits:\n");
        first_block = sb.data_bitmap_firstblock;
        bitmap_blocks = sb.data_bitmap_blocks;
        total_bits = sb.data_blocks;
        global_base = sb.data_firstblock;
    } else {
        printf("inode bitmap alloced bits:\n");
        first_block = sb.inode_bitmap_firstblock;
        bitmap_blocks = sb.inode_bitmap_blocks;
        total_bits = sb.total_inodes;
        global_base = 0;
    }

    for (uint32 block = 0; block < bitmap_blocks; block++)
    {
        uint32 bitmap_block_num = first_block + block;
        uint32 bits_in_this_block = BIT_PER_BLOCK;

        // 最后一个 block 可能不满
        if (current_bit + BIT_PER_BLOCK > total_bits)
            bits_in_this_block = total_bits - current_bit;

        buffer_t *buf = buffer_get(bitmap_block_num);

        // 遍历该 block 中的有效 bit
        for (uint32 byte = 0; byte < bits_in_this_block / BIT_PER_BYTE; byte++)
        {
            for (uint32 shift = 0; shift < BIT_PER_BYTE; shift++)
            {
                if (current_bit >= total_bits)
                    break;

                uint8 mask = (uint8)(1U << shift);
                if (buf->data[byte] & mask)
                    printf("%d ", global_base + current_bit);
                current_bit++;
            }
        }
        buffer_put(buf);
    }
    printf("over!\n\n");
}
