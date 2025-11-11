
# 系统调用号重复导致调用失败的详细分析

## 系统调用机制概述

在xv6中，系统调用是通过以下流程实现的：

1. 用户程序调用系统调用包装函数（如`unlink()`）
2. 包装函数将系统调用号放入寄存器`a7`，然后执行`ecall`指令触发异常
3. 内核处理异常，根据`a7`中的系统调用号在系统调用表中查找对应的处理函数
4. 执行对应的系统调用处理函数，并将返回值放入寄存器`a0`
5. 返回用户程序，包装函数从`a0`获取返回值

## 系统调用表

系统调用表在`kernel/syscall.c`中定义：

```c
// 系统调用处理函数指针数组
static uint64 (*syscalls[])(void) = {
  [SYS_fork]    sys_fork,
  [SYS_exit]    sys_exit,
  [SYS_wait]    sys_wait,
  // ...
  [SYS_unlink]  sys_unlink,
  [SYS_link]    sys_link,
  [SYS_mkdir]   sys_mkdir,
  [SYS_close]   sys_close,
  // ...
  [SYS_trace]   sys_trace,
  [SYS_sysinfo] sys_sysinfo,
};
```

## 问题详细分析

### 编号冲突情况

在最近的提交中，`sysnum.h`中出现了以下编号冲突：

```c
#define SYS_mknod       17   // 创建设备文件
#define SYS_getcwd      17   // 获取当前工作目录

#define SYS_unlink      18   // 删除文件
#define SYS_trace       18   // 用于调试，追踪系统调用

#define SYS_link        19   // 创建硬链接
#define SYS_sysinfo     19   // 获取通用系统信息
```

### 系统调用处理过程

当用户程序调用`unlink()`时：

1. `usys.S`中的`unlink`包装函数将`SYS_unlink`(18)放入寄存器`a7`
2. 执行`ecall`指令触发异常
3. 内核在`syscall()`函数中处理异常：
   ```c
   void syscall(void) {
     int num = myproc()->trapframe->a7; // 获取系统调用号(18)
     if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
       myproc()->trapframe->a0 = syscalls[num](); // 执行系统调用
     }
   }
   ```
4. 由于`SYS_unlink`和`SYS_trace`都定义为18，系统调用表中索引18的位置被后定义的`SYS_trace`覆盖
5. 因此，实际执行的是`sys_trace()`函数，而不是`sys_unlink()`函数

### 具体错误分析

在`copyinstr2`测试中：

```c
char b[MAXPATH+1];
for(int i = 0; i < MAXPATH; i++)
  b[i] = 'x';
b[MAXPATH] = '\0';

int ret = unlink(b); // 期望返回-1，因为文件名过长
if(ret != -1){
  printf("unlink(%s) returned %d, not -1\n", b, ret);
  exit(1);
}
```

由于系统调用号冲突，实际执行的是`sys_trace()`函数，而不是`sys_unlink()`函数。`sys_trace()`函数的返回值与`sys_unlink()`不同，导致测试失败。

### 为什么返回0而不是-1

`sys_trace()`函数的实现大致如下：

```c
uint64 sys_trace(void) {
  int mask;
  if(argint(0, &mask) < 0)
    return -1;
  
  myproc()->trace_mask = mask;
  return 0; // 成功时返回0
}
```

当`unlink(b)`被调用时，实际上是`sys_trace(b)`被执行。由于`b`被解释为`mask`参数，且不为负数，函数返回0。这就是为什么测试显示"unlink(...) returned 0, not -1"的原因。

## 修复方案

修正`sysnum.h`中的系统调用编号，确保每个系统调用都有唯一的编号：

```diff
-#define SYS_getcwd      17   // 获取当前工作目录
+#define SYS_getcwd      16   // 获取当前工作目录
-#define SYS_sysinfo     19   // 获取通用系统信息
+#define SYS_sysinfo     20   // 获取通用系统信息
-#define SYS_trace       18   // 用于调试，追踪系统调用
+#define SYS_trace       22   // 用于调试，追踪系统调用
-#define SYS_test_proc   22   // 自定义的测试调用
+#define SYS_test_proc   23   // 自定义的测试调用
```

## 结论

系统调用号重复导致系统调用表中的函数指针被覆盖，使得调用一个系统调用时实际执行的是另一个系统调用。这种情况下，返回值和行为与预期不符，导致测试失败。通过确保每个系统调用都有唯一的编号，可以解决这个问题。