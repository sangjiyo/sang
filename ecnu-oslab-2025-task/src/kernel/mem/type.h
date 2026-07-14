#pragma once
#include "../lock/type.h"


/*---------------------------------- 关于物理内存 ---------------------------------------*/

/*
    物理内存管理的核心是空闲页面组成的单链表:
    kern_reagion和user_region的list_head字段是链表的头节点
    物理页的分配就是将list_head->next移出链表
    物理页的回收就是将node插入list_head->next
    你可能注意到了:我们没有一个和物理页数量一样多的page_node数组
    那是因为:当page位于空闲页链表时, 它最前面的64bit会被特别设置为next指针, 当它被分配出去后则作为普通的空间
*/

// 物理页是最基本的资源单位, 大小设置为4KB
#define PGSIZE 4096

// 物理页节点
typedef struct page_node
{
    struct page_node *next;
} page_node_t;

// 许多物理页构成一个可分配的区域
typedef struct alloc_region
{
    uint64 begin;          // 起始物理地址
    uint64 end;            // 终止物理地址
    spinlock_t lk;         // 自旋锁(保护下面两个变量)
    uint32 allocable;      // 可分配页面数
    page_node_t list_head; // 可分配链的链头节点
} alloc_region_t;

/*
    物理内存的布局情况:
    KERNEL_BASE ~ KERNEL_DATA  内核程序kernel-qemu.elf的代码区域 (不可分配回收)
    KERNEL_DATA ~ ALLOC_BEGIN  内核程序kernel-qemu.elf的数据区域 (不可分配回收)
    ALLOC_BEGIN ~ ALLOC_END    可分配回收的区域 (前KERN_PAGES属于内核空间, 后面属于用户空间)
*/

// 内核基地址
#define KERNEL_BASE 0x80000000ul

// from kernel.ld
extern char KERNEL_DATA[];
extern char ALLOC_BEGIN[];
extern char ALLOC_END[];

// 可分配回收的区域中内核持有前KERN_PAGES个页面
#define KERN_PAGES 1024

/*---------------------------------- 关于虚拟内存 ---------------------------------------*/

/*
    内核使用RISC-V体系结构中的SV39作为虚拟内存的设计规范

    1. 页表与satp寄存器
    
    satp寄存器的bit结构: MODE(4bit) + ASID(16bit) + PPN(44bit)
    - MODE控制虚拟内存模式
    - ASID与Flash刷新有关
    - PPN存放页表基地址
    可以通过w_satp(MAKE_SATP(pgtbl))命令将页表地址填入satp寄存器并启动地址翻译
    在无页表到有页表 + 切换页表的情况下会用到

    2. SV39的虚拟地址与三级映射表

    物理页是最基本的内存资源, 有的物理页用于存放数据, 有的物理页用于存放描述"物理页映射关系"的页表
    VA: VPN[2] + VPN[1] + VPN[0] + offset
          9    +   9    +   9    +   12    = 39 (使用uint64存储) => 最大虚拟地址为512GB
    SV39使用三级页表对应三级VPN, VPN[2]称为顶级页表、VPN[1]称为次级页表、VPN[0]称为低级页表
    为什么每一级页框号是"9": 4KB/sizeof(PTE) = 512 = 2^9 所以一个物理页可以存放512个页表项
    生活中的例子: 假设你要在全国范围内找一个不认识的大学老师, 
    - 你可以先到教育部(顶级页表)查询, 得知这个老师属于大学A
    - 你接着来到大学A(次级页表)查询, 得知这个老师属于学院B
    - 你最后来到学院B(低级页表)查询, 得知这个老师属于办公室C
    - 最后你来到办公室C(物理页), 并在某个位置(offset)上找到了他

    3. 页表的基本组成单位-页表项(PTE)
    reserved + PPN[2] + PPN[1] + PPN[0] + RSW + D A G U X W R V  共64bit
       10        26       9        9       2    1 1 1 1 1 1 1 1
    需要关注的部分:
    - V : valid
    - X W R : execute write read (全0意味着这是页表所在的物理页)
    - U : 用户态是否可以访问
    - PPN区域 : 存放物理页号

*/

// 页表项和页表(页表项数组)
typedef uint64 pte_t;
typedef pte_t* pgtbl_t;

// satp寄存器相关
#define SATP_SV39 (8L << 60)                                           // MODE = SV39
#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12)) // 设置MODE和PPN字段

// 获取虚拟地址中的虚拟页(VPN)信息 占9bit
#define VA_SHIFT(level) (12 + 9 * (level))
#define VA_TO_VPN(va, level) ((((uint64)(va)) >> VA_SHIFT(level)) & 0x1FF)

// PA和PTE之间的转换
#define PA_TO_PTE(pa)  ((((uint64)(pa)) >> 12) << 10)
#define PTE_TO_PA(pte) (((uint64)(pte) >> 10) << 12)

// 页面权限控制
#define PTE_V (1 << 0) // valid
#define PTE_R (1 << 1) // read
#define PTE_W (1 << 2) // write
#define PTE_X (1 << 3) // execute
#define PTE_U (1 << 4) // user
#define PTE_G (1 << 5) // global
#define PTE_A (1 << 6) // accessed
#define PTE_D (1 << 7) // dirty

// 检查一个PTE是否属于pgtbl
#define PTE_CHECK(pte) (((pte) & (PTE_R | PTE_W | PTE_X)) == 0)

// 获取低10bit的flag信息
#define PTE_FLAGS(pte) ((pte) & 0x3FF)

// 定义一个非常大的VA, 正常来说所有VA不得大于它
#define VA_MAX (1ul << 38)
