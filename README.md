## 一、环境配置和初始代码拉取

我所做的操作系统是参考了xv6-riscv-2020，按照华东师范大学计算机科学与技术学院的操作系统课程实验https://gitee.com/xu-ke-123/ecnu-oslab-2025-task/tree/master/的实验过程和顺序来进行的。

初始的环境配置由于我之前学习MIT6.S081_xv6_labs已经搭建过了，所以在下载环境依赖和配置qemu的过程中并没有遇到什么问题。

按照建议，我创建了一个自己的github库https://github.com/sangjiyo/sang，用来保存这一次的实验代码，以及方便对每一次修改进行保存和管理。首先我就遇到了一个问题，在本地我创建的文件夹下，我直接拉取了华师大的lab1的代码框架，而后希望以此为起点，创建我自己的lab-0分支，合并提交到我自己的库中。就遇到了下面的第一个报错。

![img](pictures/1.png)

看到这个报错之后，我第一时间猜测可能是因为我直接在我的库中拉取了别人的库，合并时发生了一些冲突之类的，直接将报错信息提交给deepseek（我比较喜欢用这个的网页版）之后，果然跟我想的差不多。所以将这个仓库里的.git删掉就行了。还有一个.gitignore，用来在上传仓库的时候设置忽略哪些东西不要上传，一般是包含自己的密钥或者API-key等信息。

![img](pictures/2.png)

继续后续的上传仓库操作就没有问题了。

## 二、寻找内核启动的起点

lab-1的实验引导介绍中说了内核的启动过程是从entry.S到start.c到main.c，entry.S设置内核的起始物理地址，并为cpu分配栈空间，而后跳转到start.c开始执行，xv6-riscv-2020设计的是双核cpu启动，实现的方式是为每个cpu分配固定的空间，而后将第二颗cpu起始地址按照分配空间大小进行了一个偏移，确保两颗cpu互不干扰。

![img](pictures/3.png)

而后start.c（运行在机器模式M-mode，这个是什么）的工作主要是，将cpu从裸机状态推进到内核的监管者模式S-mode（这个又是什么），并且跳转到C语言的main()函数，关键代码和主要工作如下。

```  c
int id = r_mhartid();    // 切换到S-mode后无法访问M-mode的寄存器
w_tp(id);                // 所以需要将hartid存到可访问的寄存器tp
w_mepc((uint64)main);                  //设置返回地址
uint64 status = r_mstatus();
status &= ~MSTATUS_MPP_MASK;           
status |= MSTATUS_MPP_S;               //假装上一个模式是S-mode
w_mstatus(status);
w_satp(0);                             //明确关闭分页
asm volatile("mret");                  //触发特权级切换，进入S-mode模式，跳转到main()函数
```

而后由main()执行系统的初始化，并正式进入系统。但是在这里，我看到了一个文件kernel.ld

``` text
OUTPUT_ARCH( "riscv" )
ENTRY( _entry )
SECTIONS
{
  . = 0x80000000;
  .text : {
    *(.text .text.*)
    . = ALIGN(0x1000);
    /*
    _trampoline = .;
    *(trampsec)
    . = ALIGN(0x1000);
    ASSERT(. - _trampoline == 0x1000, "error: trampoline larger than one page");
    */
    PROVIDE(etext = .);
  }
  .rodata : {
    . = ALIGN(16);
    *(.srodata .srodata.*) /* do not need to distinguish this from .rodata */
    . = ALIGN(16);
    *(.rodata .rodata.*)
  }
  .data : {
    . = ALIGN(16);
    *(.sdata .sdata.*) /* do not need to distinguish this from .data */
    . = ALIGN(16);
    *(.data .data.*)
  }
  .bss : {
    . = ALIGN(16);
    *(.sbss .sbss.*) /* do not need to distinguish this from .bss */
    . = ALIGN(16);
    *(.bss .bss.*)
  }
  PROVIDE(end = .);
}
```



看到了它开头的ENTRY(_entry)，我觉得这个可能才是整个系统运行的起点，deepseek给出的答案如下：

kernel.ld 是 xv6 内核的链接脚本（Linker Script）。它在整个启动流程中扮演着“总设计师”的角色——虽然它本身不产生任何运行的机器码，但它决定了内核可执行文件（kernel）在内存中的最终布局，并给内核源码提供了关键的边界符号。

虽然一上来看不太懂，但是这个文件好像主要是为内核的运行制定了一些规则，我的理解是这样的。

以上，我终于对整个操作系统的开头启动过程，有了一点粗略的理解。而在华师大的lab-1中

![img](pictures/4.png)

第一个目标已经完成了，需要参考xv6的代码，填充完整当前的start.c。

## 三、利用串口驱动完成print.c

利用当前代码中的串口，仿照xv6的prntf实现，补充完整当前的print.c这里我直接将lab-1的串口文件uart.c和需要填充的print.c发送给ds让它进行填充。

![img](pictures/5.png)

而后按照它给出的代码，对print.c进行补充。而后以printf("Hello %s, pid=%d", "World", 42);为例，对printf的工作流程，和代码作用进行分析

``` 
示例：printf("Hello %s, pid=%d", "World", 42);
    va_list ap;
    int i, c;
    char *s;
    //获取锁（防止多个CPU同时打印）
    spinlock_acquire(&print_lk);          #获取锁，防止多核输出乱序
    va_start(ap, fmt);                    #首先令ap指向第一个可变参数"World"
    for (i = 0; (c = fmt[i] & 0xff) != 0; i++) {  #for循环遍历fmt指向的字符串，在遇到%
        if (c != '%') {                           #之前，都直接调用发送串口uart_putc_sync
            uart_putc_sync(c);                    #对单字符进行输出
            continue;
        }
        // 处理 '%'
        c = fmt[++i] & 0xff;           #当遇到%的时候，读取下一个字符
        if (c == 0)                    #此时为s，进入case s分支
            break;
        switch (c) {
            case 'd':
                printint(va_arg(ap, int), 10, 1);
                break;
            case 's':
                s = va_arg(ap, char *);    #va_arg将第一个参数"World"的指针赋值给s后
                if (s == 0)                #自动将ap的指向调整到下一个参数42的位置
                    s = "(null)";
                while (*s)
                    uart_putc_sync(*s++);  #逐个取出"World"的字符进行输出
                break;                     #后续的输出同上
            case '%':
                uart_putc_sync('%');
                break;
            default:
                // 未知格式，原样输出 % 和字符
                uart_putc_sync('%');
                uart_putc_sync(c);
                break;
            }
        }
    va_end(ap);                             #输出完成之后将ap置空
    spinlock_release(&print_lk);            #释放锁

```



## 四、锁的实现

在完成print的内容后，回到实验目录我发现好像spinlock.c我还没有完成，下面要对锁机制进行实现。同样参考xv6进行，只不过这一次我选择不用ds。

``` c
// 自旋锁初始化
void spinlock_init(spinlock_t *lk, char *name)
{
    lk->name = name;
    lk->locked = 0;
    lk->cpuid = 0;
}

// 是否持有自旋锁
bool spinlock_holding(spinlock_t *lk)
{
    bool r;
    r = (lk->locked && lk->cpuid == mycpuid());
    return r;

}

// 获取自旋锁
void spinlock_acquire(spinlock_t *lk)
{
    push_off();
    if (spinlock_holding(lk))
        panic("acquire");
    while (__sync_lock_test_and_set(&lk->locked, 1) != 0);
    __sync_synchronize();
    lk->cpuid = mycpuid();
}

// 释放自旋锁
void spinlock_release(spinlock_t *lk)
{
    if (!spinlock_holding(lk))
        panic("release");
    lk->cpuid = 0;
    __sync_synchronize();
    __sync_lock_release(&lk->locked);
    pop_off();
}
```



好吧，还是遇到了一些问题，在仿照xv6写到这里时，我看到了一个新函数__sync_lock_test_and_set，但是不知道它是做什么的。

![img](picture/6.png)

原来这些\__sync__*函数不是需要我自己实现的，它是编译器的内建函数，在编译过程中会直接被替换为对应的RISC-V汇编指令。所以应当是可以直接拿来使用的。

完成了上面的代码之后，lab-1的任务已经接近尾声了，再完成main.c的内容，应该就可以实现系统的启动了。

## 五、完成main.c，实现系统初次启动

阅读了xv6的main.c之后，我发现在main函数中主要实现了一些初始化，然后打印输出一些内容，但是对于我正在做的这个系统，我不太清楚需要初始化的内容有哪些，借助ai分析应当是有必要的，所以我选择将我的所以代码上传给ai，让它对照xv6的内容，告诉我我的main.c中应该写什么东西。

``` c
#include "arch/mod.h"
#include "lib/mod.h"
// 全局标志：用于主核通知从核初始化已完成
volatile int started = 0;
int main()
{
    // 获取当前 CPU 的 hartid（由 start.c 保存在 tp 中）
    int cpu_id = mycpuid();
    if (cpu_id == 0) {
        // 主核（CPU0）负责初始化串口和 printf 锁
        print_init();
        // 内存屏障，确保之前的初始化操作对其它核可见
        __sync_synchronize();
        // 通知从核初始化完成
        started = 1;
    }
    else {
        // 从核（CPU1）等待主核完成初始化
        while (!started) {
            // 空循环等待，可考虑加入 wfi 指令（但无中断时效果相同）
        }
        // 确保读到 started 的最新值
        __sync_synchronize();
    }
    // 所有核打印启动信息
    printf("cpu %d is booting!\n", cpu_id);
    // 打印完成后进入死循环（后续可扩展为调度器）
    while (1) {
        // 可加入 wfi 指令降低功耗，但无中断时无影响
        // asm volatile("wfi");
    }
    return 0;
}

```

![img](pictures/7.png)

成功了，但是cpu1先打印的输出信息，明显需要进行一些同步，这里我选择修改在cpu0初始化完成并通知cpu1之前打印输出信息。

![img](pictures/8.png)

搞定！以上lab-1的内容已经全部完成，后续的lab内容我需要加快一下速度了，先将内容上传保存到我的仓库中。
