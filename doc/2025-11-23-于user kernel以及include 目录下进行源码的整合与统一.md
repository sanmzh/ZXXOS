# 于user kernel以及include 目录下进行源码的整合与统一

## 涉及文件
user/cat.c
user/echo.c
user/forktest.c
user/wc.c
user/grind.c
user/grep.c
user/init.c
user/kill.c
user/ln.c
user/ls.c
user/mkdir.c
user/rm.c
user/stressfs.c
user/zombie.c
user/umalloc.c
kernel/bio.c
kernel/console.c
kernel/exec.c
kernel/param.h
kernel/file.c
kernel/fs.c
kernel/log.c
kernel/printf.c
kernel/sleeplock.c
kernel/string.c
kernel/syscall.c
include/elf.h
include/fcntl.h
include/file.h
include/fs.h
include/stat.h
include/types.h
include/buf.h

## 文件修改
### Makefile的修改
1.添加-Iinclude -Ikernel的文件包含路径，以确保文件目录修改后仍然能保证程序正常执行
CFLAGS += -Iinclude -Ikernel
2.分别添加-Driscv 或 -Dloongarch两个宏定义，为后续分架构编程做准备
ifeq(ARCH,loongarch)CFLAGS += -Dloongarch
ifeq(ARCH,riscv)CFLAGS += -Driscv
3.新增kernel到kernel的编译规则
$K/%.o: $K/%.c
	$(CC) $(CFLAGS) -g -c -o $@ $<
4.修改umalloc.c的编译规则
$U/umalloc.o: $U/umalloc.c
	$(CC) $(CFLAGS) -c -o $@ $<
### 对于源代码统一性进行了修改
1.根据宏定义按照架构分别包含loongarch.h以及riscv.h
#ifdef loongarch
#include "loongarch.h"
#endif
#ifdef riscv
#include "riscv.h"
#endif
2.由于底层驱动不同，对于磁盘读写函数不同架构给出不同的函数名，采用宏定义的方式创建虚拟层统一接口，以维护代码的一致性
#ifdef loongarch
#define disk_rw ramdiskrw
#endif
#ifdef riscv
#define disk_rw virtio_disk_rw
#endif
3.对函数参数使用宏定义与否进行了统一
由于open函数参数作用的修改相对较少，因此统未采用O_RDOLY参数进行修改，统一使用0来表示仅读取
4.对于printf.c文件，loongarch架构中的printf函数缺少对于%lld以及%ld情况的实现，统一使用了riscv的实现方式
5.其余的函数实现的部分差异则采用条件编译来实现以保证正确性
