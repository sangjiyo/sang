# 外部函数声明 in trap_kernel.c
.globl trap_kernel_handler

# S-mode 中断处理 (包括软件中断和外设中断)
.globl kernel_vector
.align 4
kernel_vector:

        # 准备空间给32个通用寄存器
        addi sp, sp, -256

        # 寄存器状态保存
        sd ra, 0(sp)
        sd sp, 8(sp)
        sd gp, 16(sp)
        sd tp, 24(sp)
        sd t0, 32(sp)
        sd t1, 40(sp)
        sd t2, 48(sp)
        sd s0, 56(sp)
        sd s1, 64(sp)
        sd a0, 72(sp)
        sd a1, 80(sp)
        sd a2, 88(sp)
        sd a3, 96(sp)
        sd a4, 104(sp)
        sd a5, 112(sp)
        sd a6, 120(sp)
        sd a7, 128(sp)
        sd s2, 136(sp)
        sd s3, 144(sp)
        sd s4, 152(sp)
        sd s5, 160(sp)
        sd s6, 168(sp)
        sd s7, 176(sp)
        sd s8, 184(sp)
        sd s9, 192(sp)
        sd s10, 200(sp)
        sd s11, 208(sp)
        sd t3, 216(sp)
        sd t4, 224(sp)
        sd t5, 232(sp)
        sd t6, 240(sp)

        # trap的处理过程
        call trap_kernel_handler

        # 寄存器状态恢复
        ld ra, 0(sp)
        ld sp, 8(sp)
        ld gp, 16(sp)
        # tp寄存器的值可能不可靠
        # ld tp, 24(sp)
        ld t0, 32(sp)
        ld t1, 40(sp)
        ld t2, 48(sp)
        ld s0, 56(sp)
        ld s1, 64(sp)
        ld a0, 72(sp)
        ld a1, 80(sp)
        ld a2, 88(sp)
        ld a3, 96(sp)
        ld a4, 104(sp)
        ld a5, 112(sp)
        ld a6, 120(sp)
        ld a7, 128(sp)
        ld s2, 136(sp)
        ld s3, 144(sp)
        ld s4, 152(sp)
        ld s5, 160(sp)
        ld s6, 168(sp)
        ld s7, 176(sp)
        ld s8, 184(sp)
        ld s9, 192(sp)
        ld s10, 200(sp)
        ld s11, 208(sp)
        ld t3, 216(sp)
        ld t4, 224(sp)
        ld t5, 232(sp)
        ld t6, 240(sp)

        # 栈指针恢复
        addi sp, sp, 256

        # 当前处于S-mode,返回调用者
        sret

# M-mode 中断处理 (包括时钟中断)
.globl timer_vector
.align 4
timer_vector:

        # 交换a0和mscratch寄存器的内容 (读写mscratch之类的CSR寄存器需要使用特殊指令csrrw)
        csrrw a0, mscratch, a0
        # 将寄存器 a1 a2 a3 暂存起来
        sd a1, 0(a0)      # cur_mscratch[0] = a1
        sd a2, 8(a0)      # cur_mscratch[1] = a2
        sd a3, 16(a0)     # cur_mscratch[2] = a3

        # cmp_time += INTERVAL, 以响应下一次时钟中断
        ld a1, 24(a0)     # 令a1 = cur_mscratch[3] 里面放了 CLINT_MTIMECMP(hartid)
        ld a2, 32(a0)     # 令a2 = cur_mscratch[4] 里面放了 INTERVAL        
        ld a3, 0(a1)      
        add a3, a3, a2
        sd a3, 0(a1)

        # 引发一个 S-mode software interrupt
        # 与timer_interrupt_handler函数的 w_sip(r_sip() & ~2) 互为逆过程
        li a1, 2
        csrw sip, a1

        # 从这里开始进入kernel_vector逻辑
        # kernel_vector过程
        # 从这里开始离开kernel_vector逻辑

        # 恢复寄存器 a1 a2 a3
        ld a3, 16(a0)
        ld a2, 8(a0)
        ld a1, 0(a0)
        # 交换a0和mscratch寄存器的内容以恢复原始状态
        csrrw a0, mscratch, a0

        mret
