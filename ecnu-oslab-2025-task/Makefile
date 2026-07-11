# 引入通用配置文件
include common.mk

# 配置CPU核心数量
CPUNUM = 2

# 定义目标文件输出目录
TARGET = target

# 定义各模块路径
KernelPath = src/kernel
UserPath   = src/user
MKFSPath   = src/mkfs
LoaderPath = src/loader

# 链接脚本
KERNEL_LD = $(LoaderPath)/kernel.ld
USER_LD   = $(LoaderPath)/user.ld

# 定义目标文件路径
ELFKernel  = $(TARGET)/kernel/kernel-qemu.elf
ELFUser    = $(UserPath)/initcode.h
DISKIMG    = $(TARGET)/mkfs/disk.img

# 收集内核源文件 (.c .S，包括子目录)
KernelSourceFile = $(wildcard $(KernelPath)/*.c) $(wildcard $(KernelPath)/*.S)
KernelSourceFile += $(wildcard $(KernelPath)/*/*.c) $(wildcard $(KernelPath)/*/*.S)

# 内核目标文件
KernelOBJ = $(patsubst $(KernelPath)/%.S, $(TARGET)/kernel/%.o, $(filter %.S, $(KernelSourceFile)))
KernelOBJ += $(patsubst $(KernelPath)/%.c, $(TARGET)/kernel/%.o, $(filter %.c, $(KernelSourceFile)))

# 用户程序分类: 启动程序(initcode.c) 通用库(syscall.c、help.c) 测试程序(test_1.c test_2.c ...)
USER_INIT_C = $(UserPath)/initcode.c
USER_LIB_C = $(UserPath)/syscall.c $(UserPath)/help.c
USER_TEST_C = $(filter-out $(USER_INIT_C) $(USER_LIB_C), $(wildcard $(UserPath)/*.c))

# 用户目标文件
USER_INIT_OBJ = $(TARGET)/user/initcode.o
USER_LIB_OBJ  = $(USER_LIB_C:$(UserPath)/%.c=$(TARGET)/user/%.o)
USER_TEST_OBJ = $(USER_TEST_C:$(UserPath)/%.c=$(TARGET)/user/%.o)
USER_TEST_ELF = $(USER_TEST_C:$(UserPath)/%.c=$(TARGET)/user/%.elf)

.SECONDARY: $(USER_LIB_OBJ) $(USER_TEST_OBJ)

# QEMU 模拟器配置
QEMU     = qemu-system-riscv64
QEMUOPTS = -machine virt -bios none -kernel $(ELFKernel)
QEMUOPTS += -m 128M -smp $(CPUNUM) -nographic
QEMUOPTS += -drive file=$(DISKIMG),if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

# 调试配置
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)

# 生成 .gdbinit
.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

# 创建输出目录
.PHONY: $(TARGET)
$(TARGET):
ifeq ($(wildcard $(TARGET)),)
	@mkdir -p $(TARGET)/kernel
	@mkdir -p $(TARGET)/kernel/arch
	@mkdir -p $(TARGET)/kernel/boot
	@mkdir -p $(TARGET)/kernel/lock
	@mkdir -p $(TARGET)/kernel/lib
	@mkdir -p $(TARGET)/kernel/mem
	@mkdir -p $(TARGET)/kernel/trap
	@mkdir -p $(TARGET)/kernel/proc
	@mkdir -p $(TARGET)/kernel/syscall
	@mkdir -p $(TARGET)/kernel/fs
	@mkdir -p $(TARGET)/user
	@mkdir -p $(TARGET)/mkfs
endif

# 内核编译规则
$(TARGET)/kernel/%.o: $(KernelPath)/%.S
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET)/kernel/%.o: $(KernelPath)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# 用户程序编译规则

# initcode.o（特殊）
$(USER_INIT_OBJ): $(USER_INIT_C)
	$(CC) $(CFLAGS) -I. -c -o $@ $<

# 通用库和测试程序的 .o（共享规则）
$(TARGET)/user/%.o: $(UserPath)/%.c
	$(CC) $(CFLAGS) -I. -c -o $@ $<

# 链接普通用户程序
$(TARGET)/user/%.elf: $(TARGET)/user/%.o $(USER_LIB_OBJ)
	$(LD) $(LDFLAGS) -T $(USER_LD) $< $(USER_LIB_OBJ) -o $@

# 生成 initcode.h（供内核嵌入）
$(ELFUser): $(USER_INIT_OBJ)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $(TARGET)/user/initcode.out $<
	$(OBJCOPY) -S -O binary $(TARGET)/user/initcode.out $(TARGET)/user/initcode
	xxd -i $(TARGET)/user/initcode > $(ELFUser)

# 生成 kernel-qemu.elf
$(ELFKernel): $(KernelOBJ)
	$(LD) $(LDFLAGS) -T $(KERNEL_LD) $^ -o $@

# 生成磁盘映像（包含所有普通用户程序）
$(DISKIMG): $(USER_TEST_ELF)
	gcc -Werror -Wall -I. -o $(TARGET)/mkfs/mkfs $(MKFSPath)/mkfs.c
	$(TARGET)/mkfs/mkfs $@ $(USER_TEST_ELF)

# 构建目标
.PHONY: build
build: $(TARGET) $(ELFUser) $(USER_TEST_ELF) $(ELFKernel) $(DISKIMG)
	@echo "===== make success! ====="

# 运行目标
.PHONY: run
run: build
	$(QEMU) $(QEMUOPTS)

# 调试目标
.PHONY: debug
debug: build .gdbinit
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

# 清理目标
.PHONY: clean
clean:
	rm -rf $(TARGET)
	rm -f $(UserPath)/initcode.h
	rm -f .gdbinit
