# ZXXOS: 面向教学与探索的模块化操作系统

## 概述

ZXXOS 是一个基于 xv6 的现代化教学操作系统，专注于**模块化设计**、**网络协议栈**和**安全机制**的实现。本项目不仅完整实现了现代操作系统的核心功能，还深度探索了 LoongArch 架构适配、多调度算法、系统安全等前沿主题。

## 核心特性

### 架构与核心
- **双架构支持**: 原生支持 RISC-V，并已完成对 **LoongArch 龙芯架构**的完整移植
- **模块化内核**: 清晰的内核模块划分，便于功能扩展和维护
- **增强内存管理**: 写时复制 (COW)、惰性分配、系统状态监控 (`sysinfo`)

### 进程调度
- **多调度算法**: 时间片轮转 (RR)、优先级调度、多级反馈队列 (MLFQ)
- **完整进程管理**: 支持进程创建、销毁、同步和通信的全生命周期管理

### 完整网络协议栈
- **底层驱动**: 支持 Virtio-net 和 Intel E1000 网卡
- **完整协议栈**: 实现 Ethernet、ARP、IP、ICMP、UDP、TCP 全协议栈
- **Socket API**: 完整的 BSD Socket 接口支持
- **网络工具**: `ifconfig`、`ping`、`tcpecho` 等实用网络工具

### 安全与权限
- **用户认证系统**: 完整的用户登录和权限验证机制
- **文件权限控制**: 基于用户的文件访问权限系统
- **地址空间随机化 (ASLR)**: 栈、堆、代码段地址随机化保护
- **访问控制**: 细粒度的资源访问控制机制

### 进程间通信
- **System V IPC**: 完整的共享内存、信号量、消息队列实现
- **同步原语**: 互斥锁、读写锁、信号量等多种同步机制

## 项目结构

```
ZXXOS/
├── kernel/                 # 内核核心代码
│   ├── arch/              # 架构相关代码
│   │   ├── riscv/         # RISC-V 架构实现
│   │   └── loongarch/     # LoongArch 架构实现
│   ├── fs/                # 文件系统
│   ├── net/               # 网络协议栈
│       ├── ethernet.c     # 以太网协议
│       ├── ip.c           # IP 协议
│       ├── tcp.c          # TCP 协议
│       └── socket.c       # Socket 实现
├── user/                  # 用户态程序
│   ├── net-tools/         # 网络工具
│   └── tests/             # 测试程序
├── doc/                  # 文档
└── Makefile               # 构建系统
```

## 快速开始

### 环境要求
- GCC 交叉编译工具链 (riscv64-unknown-elf-gcc)
- QEMU 模拟器 (>= 6.0)
- Python3 (用于部分测试脚本)

### 编译与运行
```bash
# 克隆项目
git clone https://gitlab.eduxiji.net/T202510004997616/project3035749-349270.git
cd ZXXOS

# 编译 RISC-V 版本
make qemu

# 编译 LoongArch 版本
make ARCH=loongarch qemu

# 运行测试套件
make test
```

### 网络功能演示
```bash
# 在 QEMU 中启动网络支持
make qemu-net

# 在系统 Shell 中使用网络工具
$ ifconfig            # 查看网络配置
$ ping 10.0.2.2       # 测试网络连通性
$ tcpecho &           # 启动 TCP 回显服务器
```

## 开发指南

### 添加新系统调用
1. 在 `kernel/syscall.c` 中注册系统调用号
2. 实现系统调用处理函数
3. 在用户态库中添加封装函数
4. 更新系统调用表

### 网络协议扩展
```c
// 示例：添加新的传输层协议
#include "net/protocol.h"

struct my_protocol {
    struct protocol proto;
    // 协议特定字段
};

int my_send(struct socket *sock, void *buf, size_t len) {
    // 实现发送逻辑
}
```

### 安全模块开发
安全模块采用插件式架构，可以轻松添加新的安全策略：
```c
#include "security/security.h"

static int my_security_check(struct task_struct *task) {
    // 实现安全检查逻辑
}

SECURITY_MODULE(my_module, my_security_check);
```

## 测试与验证

### 集成测试
项目包含完整的用户态测试套件，覆盖：
- 系统调用正确性
- 进程调度算法
- 内存管理功能
- 网络协议栈
- IPC 机制
- 安全策略

## 学习资源

### 关键文档
- `doc/xv6-riscv-book.pdf` - xv6 原版教材
- `doc/` - 各模块设计文档

### 实验任务
1. **基础实验**: 系统调用、进程调度、内存管理
2. **网络实验**: 协议分析、Socket 编程、驱动开发
3. **安全实验**: 权限控制、ASLR 实现、漏洞防护
4. **架构实验**: LoongArch 特性探索、性能优化
