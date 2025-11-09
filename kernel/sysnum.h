#ifndef _SYS_H
#define _SYS_H

// 系统调用号定义 - 整合版本

// 文件系统相关
#define SYS_getcwd     17    // 获取当前工作目录
#define SYS_dup        23    // 复制文件描述符
#define SYS_dup3       24    // 复制文件描述符到指定新描述符
#define SYS_readdir    27    // 读取目录项 [自定义]
#define SYS_mkdirat    34    // 在指定目录下创建目录
#define SYS_unlinkat   35    // 在指定目录下删除文件
#define SYS_linkat     37    // 创建文件链接
#define SYS_umount2    39    // 卸载文件系统
#define SYS_mount      40    // 挂载文件系统
#define SYS_chdir      49    // 切换工作目录
#define SYS_openat     56    // 打开或创建文件
#define SYS_close      57    // 关闭文件描述符
#define SYS_pipe2      59    // 创建管道
#define SYS_getdents64 61    // 获取目录条目
#define SYS_read       63    // 从文件描述符读取
#define SYS_write      64    // 向文件描述符写入
#define SYS_fstat      80    // 获取文件状态

// 进程管理相关
#define SYS_fork        1    // 创建子进程 [自定义]
#define SYS_wait        3    // 等待子进程结束 [自定义]
#define SYS_kill        6    // 向进程发送信号 [自定义]
#define SYS_mkdir       7    // 创建目录 [自定义]
#define SYS_sbrk       12    // 调整程序数据段大小 [自定义]
#define SYS_sleep      13    // 使进程休眠（秒） [自定义]
#define SYS_pause      34    // 暂停进程 [自定义]
#define SYS_exit       93    // 进程退出
#define SYS_nanosleep 101    // 线程睡眠（纳秒精度）
#define SYS_sched_yield 124  // 让出调度器
#define SYS_times      153   // 获取进程时间
#define SYS_uname      160   // 获取系统信息
#define SYS_gettimeofday 169 // 获取时间
#define SYS_getpid     172   // 获取进程ID
#define SYS_getppid    173   // 获取父进程ID
#define SYS_brk        214   // 修改数据段大小
#define SYS_munmap     215   // 取消内存映射
#define SYS_clone      220   // 创建子进程
#define SYS_execve     221   // 执行程序
#define SYS_mmap       222   // 内存映射
#define SYS_wait4      260   // 等待进程状态改变

// 其他系统调用
#define SYS_mknod      17    // 创建设备文件 [自定义]
#define SYS_unlink     18    // 删除文件 [自定义]
#define SYS_link       19    // 创建硬链接 [自定义]
#define SYS_trace      18    // 用于调试，追踪系统调用 [自定义]
#define SYS_dev        21    // 设备文件操作 [自定义]
#define SYS_test_proc  22    // 自定义的测试调用 [自定义]
#define SYS_rename     26    // 重命名文件或目录 [自定义]
#define SYS_remove     117   // 删除文件或目录 [自定义]
#define SYS_uptime     14    // 获取系统运行时间 [自定义]
#define SYS_sysinfo    19    // 获取通用系统信息 [自定义]
#define SYS_shutdown   210   // 关闭系统 [自定义]

// 兼容性定义（避免重复）
#define SYS_open       SYS_openat      // 打开文件（兼容旧版）
#define SYS_pipe       SYS_pipe2       // 创建管道（兼容旧版）
#define SYS_exec       SYS_execve      // 执行程序（兼容旧版）

// System calls for labs
#define SYS_kpgtbl     534				// 500 + 34 // 获取页表	// LAB_PGTBL

#define SYS_bind      529				// 500 + 
#define SYS_unbind    530
#define SYS_send      531
#define SYS_recv      532

#endif // _SYS_H