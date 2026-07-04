# LAB-1: 机器启动

## 1. 代码组织结构
```
ECNU-OSLAB-2025-TASK  
├── LICENSE        开源协议  
├── .vscode        配置了可视化调试环境
├── registers.xml  配置了可视化调试环境  
├── Makefile       编译运行整个项目  
├── common.mk      Makefile中一些工具链的定义  
├── kernel.ld      定义了内核程序在链接时的布局  
├── pictures       README使用的图片目录  
├── README.md      实验指导书  
└── src            源码
    └── kernel     内核源码
        ├── arch   RISC-V相关
        │   ├── method.h  
        │   ├── mod.h  
        │   └── type.h  
        ├── boot   机器启动
        │   ├── entry.S  
        │   └── start.c (TODO)  
        ├── lock   锁机制
        │   ├── spinlock.c (TODO)  
        │   ├── method.h  
        │   ├── mod.h  
        │   └── type.h  
        ├── lib    常用库
        │   ├── cpu.c  
        │   ├── print.c (TODO)  
        │   ├── uart.c  
        │   ├── method.h  
        │   ├── mod.h  
        │   └── type.h  
        └── main.c (TODO)  
```
## 2. 实验核心目标

完成双核的机器启动, 进入main函数并输出启动信息 (如下图)  

![alt text](pictures/01.png)

## 3. 具体任务

### 3.1 机器启动本身

要想实现上述核心目标，仔细想想只需要完成两件事

1. 让内核在在QEMU上跑起来（双核启动）：**entry.S** 到 **start.c** 到 **main.c**  

2. 让内核向屏幕输出一些字符串，也就是实现C语言中经常调用的`printf()`

第一件事需要你研究一下xv6的启动流程，只需要看到进入 **main.c** 就够了

第二件事需要你先阅读一下**uart.c**，里面包括串口（最基本的字符输入输出设备）驱动

读完之后你需要利用uart层的函数完成**print.c**中的函数，你可以参考xv6的实现，也可以自己去做

### 3.2 printf面临的资源竞争问题

串口是一种设备资源, `printf()`利用它输出字符本质是在一段时间内持有这种资源

例如, 输出`"hello,world!"`其实是连续占用串口资源12次, 调用12次`uart_putc_sync()`

假设同时存在第二个`printf()`执行流要打印`"hello,os!"`, 它就会与执行流1形成竞争关系

两条执行流交错带来的输出可能包括:

```
# 混乱的情况
hellohello,,world!os!
hheelllloo,,wosrld!!
hhello,world!ello,os!
......
# 有序的情况
hello,world!hello,os!
hello,os!hello,world!
```

我们需要一种手段, 保证`printf()`过程中, UART资源始终只被一个执行流占有同时不可抢占

生活中的例子: 公共卫生间通过"门锁"来保证马桶这一资源在一段时间内只被一人独占

影射到操作系统, 最简单的"资源锁"就是“自旋锁”, 它的实现位于**spinlock.c**

```
# 在printf中使用自旋锁的方法

spinlock_t lk;

# 锁的初始化
spinlock_init(&lk, "print_lk");

# 上锁
spinlock_acquire(&lk);

# 独占资源
uart_putc_sync();
uart_putc_sync();
......

# 解锁
spinlock_release(&lk);

```

自旋锁的可靠性依赖**开关中断**和**原子操作**这两个关键概念，你需要完全理解

- 开关中断可以保证单CPU情况下进程(执行流)切换的时候不会影响上锁操作的原子性

- 原子操作可以保证多CPU的情况下并行执行流不会同时上锁成功

完成上述工作后，你应当可以实现图片所示的效果 (在**main.c**的合适位置输出这两句话)  

## 4. 课后实验

这里有两个额外的实验帮助你理解锁的用处 

### 4.1 并行加法

``` 
    volatile static int started = 0;

    volatile static int sum = 0;

    int main()
    {
        int cpuid = r_tp();
        if(cpuid == 0) {
            print_init();
            printf("cpu %d is booting!\n", cpuid);        
            __sync_synchronize();
            started = 1;
            for(int i = 0; i < 1000000; i++)
                sum++;
            printf("cpu %d report: sum = %d\n", cpuid, sum);
        } else {
            while(started == 0);
            __sync_synchronize();
            printf("cpu %d is booting!\n", cpuid);
            for(int i = 0; i < 1000000; i++)
                sum++;
            printf("cpu %d report: sum = %d\n", cpuid, sum);
        }   
        while (1);    
    }  
```

在 **main.c** 中测试上述代码，很明显，我们的预期是后report的cpu应该告诉我们 `sum = 2000000`

但是实际结果可能是这样的  

```
cpu 0 is booting!
cpu 1 is booting!
cpu 0 report: sum = 1128497
cpu 1 report: sum = 1143332
```

考虑如何使用锁进行修正，修正后的输出可能是这样的  

```
cpu 0 is booting!
cpu 1 is booting!
cpu 0 report: sum = 1996573
cpu 1 report: sum = 2000000
```

简单说明上锁和解锁的位置不同会有什么影响（tips: 锁的粒度粗细）

### 4.2 并行输出  

尝试去掉`printf`里的锁，参考4.1的实验思路，设计测试方法使得`printf`的输出出现交错的情况  

4.1和4.2的测试代码和实验结果可以附在你的README中, 但是不要体现在你的代码里

## 5. 关于代码仓库的维护

1. 每次实验需要在上次实验的基础上继续往下做，假设你已经完成lab-0(master)

    那么你此时应该在lab-0(master)分支下使用`git checkout -b lab-1`命令创建并切换到新的分支lab-1  

    此时新建的lab-1会继承lab-0(master)的内容，但你对lab-1的修改不会影响到lab-0  

    以此类推，当你从lab-1开始走到lab-9时，你会获得越来越完整和强大的内核  

2. 你的代码仓库应该由 **代码 + Markdown文档** 两部分构成  

    文档内容不做明确要求，你有很高的自由度决定写什么和写多少

    提供一些建议: 
    
    - 本次实验新增了哪些功能，实现了什么效果

    - 对本次实验中某个过程的理解和思考

    - 本次实验和之前的实验构成什么样的逻辑联系

    - 本次实验花费的时间, 你和队友的贡献分别是什么

    - 可以使用markdown的分层分点来增加条理性，便于别人阅读和抓住重点

    **总之，这是你的代码仓库，请对你自己的代码和文档负责**  
    
    **注意，代码是继承和连续发展的, 但文档不是，每次的文档都是全新一页**  

3. 提醒: 之所以要求大家维护代码仓库，是为了查看大家的提交记录

    所以请及时同步当天写的代码到线上仓库，不要攒到最后一口气提交，否则可能被误判为不当行为
