K=kernel
U=user
R=riscv
L=loongarch
I=include
ARCH?=riscv
A=$(ARCH)

OBJS = \
  $K/entry.o \
  $K/main.o \
  $K/uart.o \
  $K/printf.o \
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
  $K/sysfile.o

# 架构特定的 OBJS
ifeq ($(ARCH),riscv)
OBJS += \
  $K/start.o \
  $K/trampoline.o \
  $K/plic.o \
  $K/virtio_disk.o \
  $K/e1000.o \
  $K/net.o \
  $K/pci.o \
  $K/stats.o \
  $K/sprintf.o \
  $K/kernelvec.o
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
CFLAGS += -fno-builtin-free
CFLAGS += -fno-builtin-memcpy -Wno-main
CFLAGS += -fno-builtin-printf -fno-builtin-fprintf -fno-builtin-vprintf
CFLAGS += -I. -I$A -I$A/$K -I$I -I$K
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
CFLAGS += -Driscv
CFLAGS += -DNET_TESTS_PORT=$(SERVERPORT)		# LAB_NET

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
CFLAGS += -I. -I$A/$K -I$I -I$K -fno-stack-protector
CFLAGS += -fno-pie -no-pie
CFLAGS += -Dloongarch

endif

LDFLAGS = -z max-page-size=4096

KERNEL_DEPS = $(OBJS) $A/$K/kernel.ld
USER_DEPS = $U/%.o $(ULIB)
ifeq ($(ARCH),loongarch)
KERNEL_DEPS += $U/initcode
USER_WAYS = $(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^
endif
ifeq ($(ARCH),riscv)
USER_WAYS = $(LD) $(LDFLAGS) -T $A/$U/user.ld -o $@ $< $(ULIB)
USER_DEPS += $A/$U/user.ld
endif

$K/kernel: $(KERNEL_DEPS)
	mkdir -p $K
	$(LD) $(LDFLAGS) -T $A/$K/kernel.ld -o $K/kernel $(OBJS)
	$(OBJDUMP) -S $K/kernel > $K/kernel.asm
	$(OBJDUMP) -t $K/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $K/kernel.sym

$K/%.o: $A/$K/%.S
	$(CC) $(CFLAGS) -g -c -o $@ $<

$K/%.o: $A/$K/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$K/%.o: $K/%.c
	$(CC) $(CFLAGS) -g -c -o $@ $<

ifeq ($(ARCH),riscv)
$K/start.o: $A/$K/start.c
	$(CC) $(CFLAGS) -c -o $@ $<
tags: $(OBJS)
	etags riscv/kernel/*.S riscv/kernel/*.c
endif
ifeq ($(ARCH),loongarch)
$K/%.o: $K/%.S
	$(CC) $(CFLAGS) -g -c -o $@ $<

$U/initcode: $A/$U/initcode.S
	$(CC) $(CFLAGS) -nostdinc -I. -Ikernel -c $A/$U/initcode.S -o $U/initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $U/initcode.out $U/initcode.o
	$(OBJCOPY) -S -O binary $U/initcode.out $U/initcode
	$(OBJDUMP) -S $U/initcode.o > $U/initcode.asm

tags: $(OBJS) _init
	etags *.S *.c
endif

ULIB = $U/ulib.o $U/usys.o $U/printf.o $U/umalloc.o

$U/ulib.o: $A/$U/ulib.c
	$(CC) $(CFLAGS) -c -o $@ $<

$U/printf.o: $A/$U/printf.c
	$(CC) $(CFLAGS) -c -o $@ $<

$U/umalloc.o: $A/$U/umalloc.c
	$(CC) $(CFLAGS) -c -o $@ $<

ifeq ($(ARCH),riscv)
# ifeq ($(LAB),lock)
ULIB += $U/statistics.o
# endif
endif

$U/_%:  $(USER_DEPS)
	$(USER_WAYS)
	$(OBJDUMP) -S $@ > $U/$*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $U/$*.sym
$U/usys.S : $A/$U/usys.pl
	perl $A/$U/usys.pl > $U/usys.S
$U/usys.o : $U/usys.S
	$(CC) $(CFLAGS) -c -o $U/usys.o $U/usys.S
$U/_forktest: $U/forktest.o $(ULIB)
	# forktest has less library code linked in - needs to be small
	# in order to be able to max out the proc table.
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $U/_forktest $U/forktest.o $U/ulib.o $U/usys.o
	$(OBJDUMP) -S $U/_forktest > $U/forktest.asm
$U/%.o: $A/$U/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$U/%.o: $A/$U/%.S
	$(CC) $(CFLAGS) -c -o $@ $<

ifeq ($(ARCH),riscv)
# 修改用户程序编译规则，从 riscv/user/ 找源文件
mkfs/mkfs: $A/mkfs/mkfs.c $A/$K/fs.h $K/param.h
	gcc -Wno-unknown-attributes -I. -I$A -o mkfs/mkfs $A/mkfs/mkfs.c
endif
ifeq ($(ARCH),loongarch)
mkfs/mkfs: $A/mkfs/mkfs.c $A/$K/fs.h $K/param.h
	gcc -Werror -Wall -I. -o mkfs/mkfs $A/mkfs/mkfs.c
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
	$U/_stressfs\
	$U/_usertests\
#	$U/_grind\
	$U/_wc\
	$U/_zombie\

ifeq ($(ARCH),riscv)
UPROGS += $U/_grind\
	$U/_logstress\
	$U/_forphan\
	$U/_dorphan\
	$U/_trace\
	$U/_sysinfotest\
	$U/_pgtbltest\
	$U/_bttest\
	$U/_cowtest\
	$U/_nettest\
	$U/_kalloctest\
	$U/_rwlktest\
	$U/_mmaptest\

fs.img: mkfs/mkfs README $(UPROGS)
	mkfs/mkfs fs.img README $(UPROGS)

-include riscv/kernel/*.d riscv/user/*.d

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)
ifndef CPUS
CPUS := 4
endif

QEMUOPTS = -machine virt -bios none -kernel $K/kernel -m 128M -smp $(CPUS) -nographic
QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

# LAB_NET
FWDPORT1 = $(shell expr `id -u` % 5000 + 25999)
FWDPORT2 = $(shell expr `id -u` % 5000 + 30999)

# LAB_NET
QEMUOPTS += -netdev user,id=net0,hostfwd=udp::$(FWDPORT1)-:2000,hostfwd=udp::$(FWDPORT2)-:2001 -object filter-dump,id=net0,netdev=net0,file=packets.pcap
QEMUOPTS += -device e1000,netdev=net0,bus=pcie.0

qemu: check-qemu-version $K/kernel fs.img
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
SH_FLAGS = -O -fno-omit-frame-pointer -ggdb -MD -march=loongarch64 -mabi=lp64s -ffreestanding -fno-common -nostdlib -I. -fno-stack-protector -fno-pie -no-pie -c -o

$U/_sh: $A/$U/sh.c $(ULIB)
	$(CC) $(SH_FLAGS) $U/sh.o $A/$U/sh.c
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

-include kernel/*.d user/*.d

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

clean:
	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg \
		*.asm *.sym *.d packets.pcap \
		$K/*.o $K/*.d $K/*.asm $K/*.sym \
		$U/*.o $U/*.d $U/*.asm $U/*.sym $U/_* \
		$K/ramdisk.h $U/initcode $U/initcode.out \
		$K/kernel fs.img \
		mkfs/mkfs .gdbinit \
		$U/usys.S \
		$(UPROGS) \
		$R/$K/*.o $R/$K/*.d $R/$K/*.asm $R/$K/*.sym $R/$K/tags \
		$R/$U/*.o $R/$U/*.d $R/$U/*.asm $R/$U/*.sym \
		$R/$K/kernel $R/$U/usys.S \
		$R/mkfs/mkfs \
		$L/$K/kernel-back fs.img \
		$L/$K/*.o $L/$K/*.d $L/$K/*.Lsm $L/$K/*.sym $A/$K/tags \
		$L/$U/*.o $L/$U/*.d $L/$U/*.asm $L/$U/*.sym \
		$L/$K/kernel $L/$U/usys.S \
		$L/mkfs/mkfs \
		$L/$K/kernel-back fs.img \
