
# ZXXOS Makefile 修改文档

把riscv部分移入riscv目录以便于后续loongarch架构的加入

## 概述
本文档记录了对 ZXXOS 项目中 Makefile 的修改，主要是为了修复用户程序编译时的头文件路径问题，并添加了完整的清理规则。

## 修改内容

### 1. 修复用户程序编译时的头文件路径问题

#### 问题
用户程序编译时找不到头文件，错误信息：
```
riscv/user/ulib.c:1:10: fatal error: kernel/types.h: No such file or directory
```

#### 解决方案
1. 添加了`USER_CFLAGS`变量，继承自`CFLAGS`并添加了`-I$R`选项：
```makefile
# 为用户程序添加头文件搜索路径
USER_CFLAGS = $(CFLAGS) -I$R
```

2. 修改了所有用户程序编译规则，从使用`CFLAGS`改为使用`USER_CFLAGS`：
```makefile
$U/ulib.o: $R/$U/ulib.c
	$(CC) $(USER_CFLAGS) -c -o $@ $<

$U/printf.o: $R/$U/printf.c
	$(CC) $(USER_CFLAGS) -c -o $@ $<

$U/umalloc.o: $R/$U/umalloc.c
	$(CC) $(USER_CFLAGS) -c -o $@ $<

$U/usys.o : $U/usys.S
	$(CC) $(USER_CFLAGS) -c -o $U/usys.o $U/usys.S

# 修改用户程序编译规则，从 riscv/user/ 找源文件
$U/%.o: $R/$U/%.c
	$(CC) $(USER_CFLAGS) -c -o $@ $<

$U/%.o: $R/$U/%.S
	$(CC) $(USER_CFLAGS) -c -o $@ $<
```

3. 保留了内核文件编译规则使用原来的`CFLAGS`：
```makefile
$K/%.o: $R/$K/%.c
	$(CC) $(CFLAGS) -c -o $@ $<
```

### 2. 添加完整的清理规则

#### 问题
原始Makefile缺少clean规则，导致编译过程中生成的中间文件无法清理。

#### 解决方案
添加了完整的clean规则，可以清理根目录和riscv目录下的所有中间文件：

```makefile
clean:
	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg \
		*.asm *.sym *.d packets.pcap \
		$K/*.o $K/*.d $K/*.asm $K/*.sym \
		$U/*.o $U/*.d $U/*.asm $U/*.sym \
		$K/kernel fs.img \
		mkfs/mkfs .gdbinit \
		$U/usys.S \
		$(UPROGS) \
		$R/$K/*.o $R/$K/*.d $R/$K/*.asm $R/$K/*.sym \
		$R/$U/*.o $R/$U/*.d $R/$U/*.asm $R/$U/*.sym \
		$R/$K/kernel $R/$U/usys.S
```

这个clean规则会删除以下文件和目录：
1. LaTeX生成的临时文件（*.tex, *.dvi, *.idx等）
2. 根目录下的汇编文件、符号文件、依赖文件和网络抓包文件（*.asm *.sym *.d packets.pcap）
3. 内核目录下的目标文件、依赖文件、汇编文件和符号文件
4. 用户目录下的目标文件、依赖文件、汇编文件和符号文件
5. 内核镜像文件和文件系统镜像
6. mkfs工具和.gdbinit文件
7. 用户程序生成的usys.S文件
8. 所有用户程序
9. riscv目录下的内核目标文件、依赖文件、汇编文件和符号文件
10. riscv目录下的用户程序目标文件、依赖文件、汇编文件和符号文件
11. riscv目录下的内核镜像和用户程序生成的usys.S文件

## 使用方法

### 编译项目
```bash
make qemu
```

### 清理项目
```bash
make clean
```

## 注意事项

1. 这些修改确保了用户程序在编译时能从riscv目录下查找头文件，而内核程序仍然使用原来的搜索路径。
2. clean规则现在可以完全清理项目中的所有中间文件，包括根目录和riscv目录下的文件。
3. 如果将来添加新的用户程序或内核模块，请确保它们的编译规则使用正确的编译标志（用户程序使用USER_CFLAGS，内核程序使用CFLAGS）。