K=kernel
U=user
R=riscv
L=loongarch
I=include
AR=arch
ARCH?=riscv
A=$(ARCH)
N=$K/net
P=$N/platform/xv6-riscv

# 定义目录
USER_DIR = user
MKFS_DIR = mkfs
DIRS = $(USER_DIR) $(MKFS_DIR)

# 在Makefile解析时自动创建目录
$(shell mkdir -p $(DIRS))

OBJS = \
  $K/entry.o \
  $K/main.o \
  $K/uart.o \
  $K/printf.o \
  $K/printfmt.o \
  $K/proc.o \
  $K/spinlock.o \
  $K/string.o \
  $K/swtch.o \
  $K/console.o \
  $K/sleeplock.o \
  $K/file.o \
  $K/kalloc.o\
  $K/vm.o\
  $K/trap.o\
  $K/bio.o\
  $K/log.o\
  $K/fs.o\
  $K/pipe.o\
  $K/exec.o\
  $K/syscall.o\
  $K/sysproc.o\
  $K/sysfile.o\
  $K/ipc/systemV/sem.o \
  $K/ipc/systemV/syssem.o


# 架构特定的 OBJS
ifeq ($(ARCH),riscv)
OBJS += \
  $K/start.o \
  $K/trampoline.o \
  $K/plic.o \
  $K/rtc.o \
  $K/time.o \
  $K/virtio_disk.o \
  $K/syssocket.o \
  $N/util.o \
  $N/net.o \
  $N/ether.o \
  $N/ip.o \
  $N/arp.o \
  $N/icmp.o \
  $N/udp.o \
  $N/tcp.o \
  $N/socket.o \
  $P/virtio_net.o \
  $P/std.o \
  $K/e1000.o \
  $K/E1000_net.o \
  $K/pci.o \
  $K/stats.o \
  $K/kernelvec.o \
  $K/ipc/systemV/shm.o \
  $K/ipc/systemV/msg.o \
  $K/ipc/systemV/sysshm.o \
  $K/ipc/systemV/sysmsg.o
endif

ifeq ($(ARCH),loongarch)
OBJS += \
  $K/tlbrefill.o \
  $K/merror.o \
  $K/apic.o \
  $K/extioi.o \
  $K/ramdisk.o \
  $K/uservec.o \
  $K/kernelvec.o
endif

# 工具链配置
ifeq ($(ARCH),riscv)
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if riscv64-unknown-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-elf-'; \
	elif riscv64-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-elf-'; \
	elif riscv64-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-linux-gnu-'; \
	elif riscv64-unknown-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-linux-gnu-'; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find a riscv64 version of GCC/binutils." 1>&2; \
	echo "*** To turn off this error, run 'gmake TOOLPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif
QEMU = qemu-system-riscv64
MIN_QEMU_VERSION = 7.2
endif

ifeq ($(ARCH),loongarch)
export CC_PREFIX = /opt/cross-tools
export PATH := $(CC_PREFIX)/bin:$(PATH)
export LD_LIBRARY_PATH := $(CC_PREFIX)/lib:$(CC_PREFIX)/loongarch64-unknown-linux-gnu/lib:$(LD_LIBRARY_PATH)
UNAME_M=$(shell uname -m)
ifeq ($(findstring loongarch64,$(UNAME_M)),loongarch64)
TOOLPREFIX ?= 
else
TOOLPREFIX = loongarch64-unknown-linux-gnu-
endif
endif

CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

ifeq ($(ARCH),riscv)
# endif
CFLAGS = -Wall -Werror -Wno-unknown-attributes -O -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -march=rv64gc
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding
CFLAGS += -fno-common -nostdlib
CFLAGS += -fno-builtin-strncpy -fno-builtin-strncmp -fno-builtin-strlen -fno-builtin-memset
CFLAGS += -fno-builtin-memmove -fno-builtin-memcmp -fno-builtin-log -fno-builtin-bzero
CFLAGS += -fno-builtin-strchr -fno-builtin-exit -fno-builtin-malloc -fno-builtin-putc
CFLAGS += -fno-builtin-free -fno-builtin-strnlen -fno-builtin-snprintf -fno-builtin-vsnprintf
CFLAGS += -fno-builtin-memcpy -Wno-main
CFLAGS += -fno-builtin-printf -fno-builtin-fprintf -fno-builtin-vprintf
CFLAGS += -I. -I $(AR)/$A -I $(AR)/$A/$K -I$I -I$K -I$I/$U
+CFLAGS += -I $N -I $P
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
CFLAGS += -Driscv
CFLAGS += -DNET_TESTS_PORT=$(SERVERPORT)		# LAB_NET
# CFLAGS += -DSCHEDULER_RR				# 启用RR调度器
# CFLAGS += -DSCHEDULER_PRIORITY			# 启用优先级调度器
# CFLAGS += -DSCHEDULER_MLFQ			# 启用MLFQ调度器

# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

endif

ifeq ($(ARCH),loongarch)

ASFLAGS = -march=loongarch64 -mabi=lp64s
CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb
CFLAGS += -MD
CFLAGS += -march=loongarch64 -mabi=lp64s
CFLAGS += -ffreestanding -fno-common -nostdlib
CFLAGS += -I. -I $(AR)/$A/$K -I$I -I$K -I $(AR)/$A -fno-stack-protector
CFLAGS += -fno-pie -no-pie
CFLAGS += -Dloongarch

endif

CFLAGS += -I$I/$K -I$(AR)
LDFLAGS = -z max-page-size=4096

KERNEL_DEPS = $(OBJS)  $(AR)/$A/$K/kernel.ld
USER_DEPS = $U/%.o $(ULIB)
ifeq ($(ARCH),loongarch)
KERNEL_DEPS += $U/initcode
USER_WAYS = $(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^
endif
ifeq ($(ARCH),riscv)
USER_WAYS = $(LD) $(LDFLAGS) -T  $(AR)/$A/$U/user.ld -o $@ $< $(ULIB)
USER_DEPS +=  $(AR)/$A/$U/user.ld
endif

$K/kernel: $(KERNEL_DEPS)
	mkdir -p $K
	$(LD) $(LDFLAGS) -T  $(AR)/$A/$K/kernel.ld -o $K/kernel $(OBJS)
	$(OBJDUMP) -S $K/kernel > $K/kernel.asm
	$(OBJDUMP) -t $K/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $K/kernel.sym

$K/%.o:  $(AR)/$A/$K/%.S
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -g -c -o $@ $<

$K/%.o:  $(AR)/$A/$K/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$K/%.o: $K/%.c
	$(CC) $(CFLAGS) -g -c -o $@ $<

# 将 arch/riscv/kernel/net 目录下的源文件编译到 kernel/net 目录
kernel/net/%.o: arch/riscv/kernel/net/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# 将 arch/riscv/kernel/net/platform/xv6-riscv 目录下的源文件编译到 kernel/net/platform/xv6-riscv 目录
kernel/net/platform/xv6-riscv/%.o: arch/riscv/kernel/net/platform/xv6-riscv/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<


ifeq ($(ARCH),riscv)
$K/start.o:  $(AR)/$A/$K/start.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<
tags: $(OBJS)
	etags riscv/kernel/*.S riscv/kernel/*.c
endif
ifeq ($(ARCH),loongarch)
$K/%.o: $K/%.S
	$(CC) $(CFLAGS) -g -c -o $@ $<

$U/initcode:  $(AR)/$A/$U/initcode.S
	$(CC) $(CFLAGS) -nostdinc -I. -Ikernel -c  $(AR)/$A/$U/initcode.S -o $U/initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $U/initcode.out $U/initcode.o
	$(OBJCOPY) -S -O binary $U/initcode.out $U/initcode
	$(OBJDUMP) -S $U/initcode.o > $U/initcode.asm

tags: $(OBJS) _init
	etags *.S *.c
endif

ULIB = $U/ulib.o $U/usys.o $U/printf.o $U/umalloc.o 

$U/ulib.o: $(AR)/$A/$U/ulib.c
	$(CC) $(CFLAGS) -c -o $@ $<

$U/printf.o: $(AR)/$A/$U/printf.c
	$(CC) $(CFLAGS) -c -o $@ $<

$U/umalloc.o: $U/umalloc.c
	$(CC) $(CFLAGS) -c -o $@ $<

ifeq ($(ARCH),riscv)
# ifeq ($(LAB),lock)
ULIB += $U/statistics.o $U/pwd.o $U/hash.o
# endif
endif

$U/_%:  $(USER_DEPS)
	$(USER_WAYS)
	$(OBJDUMP) -S $@ > $U/$*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $U/$*.sym
$U/usys.S :  $(AR)/$A/$U/usys.pl
	perl  $(AR)/$A/$U/usys.pl > $U/usys.S
$U/usys.o : $U/usys.S
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $U/usys.o $U/usys.S
$U/_forktest: $U/forktest.o $(ULIB)
	# forktest has less library code linked in - needs to be small
	# in order to be able to max out the proc table.
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $U/_forktest $U/forktest.o $U/ulib.o $U/usys.o
	$(OBJDUMP) -S $U/_forktest > $U/forktest.asm
$U/%.o:  $(AR)/$A/$U/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$U/%.o:  $(AR)/$A/$U/%.S
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

ifeq ($(ARCH),riscv)
# 修改用户程序编译规则，从 riscv/user/ 找源文件
mkfs/mkfs:  $(AR)/$A/mkfs/mkfs.c $I/$K/fs.h $K/param.h
	gcc -Wno-unknown-attributes -I. -I$I -Driscv -o mkfs/mkfs  $(AR)/$A/mkfs/mkfs.c
endif
ifeq ($(ARCH),loongarch)
mkfs/mkfs:  $(AR)/$A/mkfs/mkfs.c $I/$K/fs.h $K/param.h
	gcc -Werror -Wall -I. -I$I -Dloongarch -o mkfs/mkfs  $(AR)/$A/mkfs/mkfs.c
endif
UPROGS=\
	$U/_cat\
	$U/_echo\
	$U/_forktest\
	$U/_grep\
	$U/_init\
	$U/_kill\
	$U/_ln\
	$U/_ls\
	$U/_mkdir\
	$U/_rm\
	$U/_sh\
#	$U/_stressfs\
	$U/_trace\
	$U/_sysinfotest\
	$U/_usertests\
	$U/_semtest\
	$U/_cowtest\
	$U/_grind\
        $U/_wc\
	$U/_zombie\
	
ifeq ($(ARCH),riscv)
UPROGS += $U/_grind\
    $U/_wc\
	$U/_zombie\
	$U/_login\
	$U/_useradd\
	$U/_pwdtests\
	$U/_whoami\
	$U/_chown\
	$U/_chmod\
	$U/_perms_test\
	$U/_aslrtest\
#	$U/_rrtest\
	$U/_prioritytest\
	$U/_test_proc_mlfq\
#	$U/_tcpecho\
	$U/_udpecho\
	$U/_ifconfig\
	$U/_logstress\
	$U/_forphan\
	$U/_dorphan\
	$U/_pgtbltest\
	$U/_bttest\
	$U/_kalloctest\
	$U/_rwlktest\
	$U/_mmaptest\
	$U/_shmtest\
	$U/_msgtest\


fs.img: mkfs/mkfs README $(UPROGS)
	mkfs/mkfs fs.img README $(UPROGS)

# 递归 include $K 和 $U 目录下的所有 .d 文件
-include $(shell find $K $U -name "*.d")

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)
ifndef CPUS
CPUS := 1
endif

TAPDEV=tap0
TAPADDR=192.0.2.1/24

QEMUOPTS = -machine virt -bios none -kernel $K/kernel -m 128M -smp $(CPUS) -nographic
QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
QEMUOPTS += -netdev tap,ifname=$(TAPDEV),id=en0
QEMUOPTS += -device virtio-net-device,netdev=en0,csum=off,gso=off,guest_csum=off,bus=virtio-mmio-bus.1

tap:
	@ip addr show $(TAPDEV) 2>/dev/null || (echo "Create '$(TAPDEV)'"; \
		sudo ip tuntap add mode tap user $(USER) name $(TAPDEV); \
		sudo sysctl -w net.ipv6.conf.$(TAPDEV).disable_ipv6=1; \
		sudo ip addr add $(TAPADDR) dev $(TAPDEV); \
		sudo ip link set $(TAPDEV) up; \
		ip addr show $(TAPDEV); \
	)
# LAB_NET
FWDPORT1 = $(shell expr `id -u` % 5000 + 25999)
FWDPORT2 = $(shell expr `id -u` % 5000 + 30999)

# LAB_NET
QEMUOPTS += -netdev user,id=net0,hostfwd=udp::$(FWDPORT1)-:2000,hostfwd=udp::$(FWDPORT2)-:2001 -object filter-dump,id=net0,netdev=net0,file=packets.pcap
QEMUOPTS += -device e1000,netdev=net0,bus=pcie.0

qemu: check-qemu-version $K/kernel fs.img tap
	$(QEMU) $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb: $K/kernel .gdbinit fs.img
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

# LAB_NET
# try to generate a unique port for the echo server
SERVERPORT = $(shell expr `id -u` % 5000 + 25099)

print-gdbport:
	@echo $(GDBPORT)

QEMU_VERSION := $(shell $(QEMU) --version | head -n 1 | sed -E 's/^QEMU emulator version ([0-9]+\.[0-9]+)\..*/\1/')
check-qemu-version:
	@if [ "$(shell echo "$(QEMU_VERSION) >= $(MIN_QEMU_VERSION)" | bc)" -eq 0 ]; then \
		echo "ERROR: Need qemu version >= $(MIN_QEMU_VERSION)"; \
		exit 1; \
	fi
	
endif

ifeq ($(ARCH),loongarch)
SH_FLAGS = -O -fno-omit-frame-pointer -ggdb -MD -march=loongarch64 -mabi=lp64s -ffreestanding -fno-common -nostdlib -I. -I$I -fno-stack-protector -fno-pie -no-pie -c -o

$U/_sh:  $(AR)/$A/$U/sh.c $(ULIB)
	$(CC) $(SH_FLAGS) $U/sh.o  $(AR)/$A/$U/sh.c
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $U/_sh $U/sh.o $(ULIB)
	$(OBJDUMP) -S $U/_sh > $U/sh.asm

# Prevent deletion of intermediate files, e.g. cat.o, after first build, so
# that disk image changes after first build are persistent until clean.  More
# details:
# http://www.gnu.org/software/make/manual/html_node/Chained-Rules.html
.PRECIOUS: %.o

fs.img: mkfs/mkfs README $(UPROGS)
	mkfs/mkfs fs.img README $(UPROGS)
	xxd -i fs.img > kernel/ramdisk.h

-include $K/*.d $U/*.d $N/*.d $P/*.d

QEMU = qemu-system-loongarch64
QEMU_OPTS = -kernel kernel/kernel -m 1G -nographic -smp 1
QEMU_OPTS += -drive file=fs.img,if=none,format=raw,id=x0
QEMU_OPTS += -device virtio-blk-pci,drive=x0
QEMU_OPTS += -no-reboot
QEMU_OPTS += -device virtio-net-pci,netdev=net0
QEMU_OPTS += -netdev user,id=net0
QEMU_OPTS += -rtc base=utc

all: fs.img $K/kernel 

qemu: all
	$(QEMU) $(QEMU_OPTS)
	
endif

# ramdisk.h is 生成文件
clean:
	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg \
		packets.pcap \
		$K/kernel fs.img \
		mkfs/mkfs .gdbinit \
		$U/usys.S \
		$(UPROGS) \
		$(AR)/$L/$K/ramdisk.h $(AR)/$R/$K/ramdisk.h $U/initcode $(AR)/$L/$U/initcode.out $(AR)/$R/$U/initcode.out $U/initcode.out $K/ramdisk.h\
		$(AR)/$R/$K/kernel $(AR)/$R/$U/usys.S \
		$(AR)/$R/mkfs/mkfs \
		$(AR)/$L/$K/kernel-back fs.img \
		$(AR)/$L/$K/kernel $(AR)/$L/$U/usys.S \
		$(AR)/$L/mkfs/mkfs \
		$(AR)/$L/$K/kernel-back fs.img
	rm -f user/_*	# 删除 user 目录下的所有 _* 文件
	# 递归删除 kernel 和 user 目录下的中间文件
	find $K $U -type f \( -name "*.o" -o -name "*.d" -o -name "*.asm" -o -name "*.sym" -o -name "tags" \) -delete

