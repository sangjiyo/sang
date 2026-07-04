# 引入通用配置文件
include common.mk

# 配置CPU核心数量
CPUNUM = 2
# 定义目标文件输出目录
TARGET = target
# 定义各模块路径
KernelPath = src/kernel
UserPath = src/user
# 内核链接脚本
KERNEL_LD  = kernel.ld
# 定义内核目标文件路径
ELFKernel = $(TARGET)/kernel/kernel-qemu.elf
NakedKernel = $(TARGET)/kernel/kernel-qemu.bin

# 收集内核源代码文件（.c和.S汇编文件）
KernelSourceFile = $(wildcard $(KernelPath)/*.c) $(wildcard $(KernelPath)/*.S)
KernelSourceFile += $(wildcard $(KernelPath)/*/*.c) $(wildcard $(KernelPath)/*/*.S)
# 收集用户态源代码文件
UserSourceFile = $(wildcard $(UserPath)/*.c)

# 生成目标文件（.o）路径列表
KernelOBJ = $(patsubst $(KernelPath)/%.S, $(TARGET)/kernel/%.o, $(filter %.S, $(KernelSourceFile)))
KernelOBJ += $(patsubst $(KernelPath)/%.c, $(TARGET)/kernel/%.o, $(filter %.c, $(KernelSourceFile)))
UserOBJ = $(patsubst $(UserPath)/%.c, $(TARGET)/user/%.o, $(filter %.c, $(UserSourceFile)))

# QEMU模拟器配置
QEMU     = qemu-system-riscv64  # 指定QEMU程序
QEMUOPTS = -machine virt -bios none -kernel $(TARGET)/kernel/kernel-qemu.elf  # 基础启动参数
QEMUOPTS += -m 128M -smp $(CPUNUM) -nographic  # 内存、CPU数量及无图形界面配置

# 调试相关配置
GDBPORT = $(shell expr `id -u` % 5000 + 25000)  # 动态计算GDB端口号
# 根据QEMU版本选择合适的GDB调试参数
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)

# 生成GDB初始化文件
.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

# 运行目标：先构建再启动QEMU
run: build
	$(QEMU) $(QEMUOPTS)

# 调试目标：启动带GDB调试的QEMU
debug: $(KERN) .gdbinit
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

# 构建目标：创建输出目录并编译内核
build: $(TARGET) $(ELFKernel) 

# 创建输出目录结构（如果不存在）
.PHONY: $(TARGET)
$(TARGET):
ifeq ($(wildcard $(TARGET)),)
	@mkdir -p $(TARGET)/kernel
	@mkdir -p $(TARGET)/kernel/arch
	@mkdir -p $(TARGET)/kernel/boot
	@mkdir -p $(TARGET)/kernel/lock
	@mkdir -p $(TARGET)/kernel/lib
endif

# 编译规则：将汇编文件(.S)编译为目标文件(.o)
$(TARGET)/kernel/%.o: $(KernelPath)/%.S
	$(CC) $(CFLAGS) -c -o $@ $<

# 编译规则：将C文件(.c)编译为目标文件(.o)
$(TARGET)/kernel/%.o: $(KernelPath)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# 编译用户态程序
$(TARGET)/user/%.o: $(UserPath)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# 链接生成内核ELF文件
$(ELFKernel): $(KernelOBJ) $(UserOBJ)
	$(LD) $(LDFLAGS) -T $(KERNEL_LD) $^ -o $@

# 清理目标：删除输出目录
.PHONY: clean
clean:
	rm -rf target