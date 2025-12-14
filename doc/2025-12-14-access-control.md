# Access Control 实现文档

## 概述

本文档描述了在xv6操作系统中实现的访问控制机制，包括用户身份验证、文件权限控制和用户权限管理等功能。这些实现遵循Unix风格的权限模型，同时根据项目需求进行了适当的调整。

## 1. 用户身份验证

### 1.1 用户账户抽象

在`user/pwd.h`中实现了POSIX兼容的用户账户抽象：

```c
struct passwd {
  char *pw_name;   // 用户名
  char *pw_passwd;  // 密码（哈希值+盐值）
  uint pw_uid;      // 用户ID
  uint pw_gid;      // 组ID
  char *pw_gecos;   // 用户信息
  char *pw_dir;     // 主目录
  char *pw_shell;   // 默认shell
};
```

### 1.2 密码文件

用户账户信息存储在`/etc/passwd`文件中，格式为：
```
username:hash$salt:uid:gid:gecos:dir:shell
```

例如：
```
root:$1321744428$32284:0:0:root:/root:/sh
sanm:$-1199158876$19274:1000:1000:sanm:/home/sanm:/sh
```

### 1.3 密码哈希

使用Jenkins `one_at_a_time`哈希函数对密码进行哈希处理，并添加32位盐值：

```c
uint jenkins_one_at_a_time_hash(const char* key, uint length) {
    uint i = 0;
    uint hash = 0;
    while (i != length) {
        hash += key[i++];
        hash += hash << 10;
        hash ^= hash >> 6;
    }
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash;
}
```

密码存储格式为：`$hash$salt`，其中hash是密码与盐值连接后的哈希值。

### 1.4 用户管理命令

#### useradd命令
创建新用户账户：
```c
// 提示输入用户名
// 提示输入密码（不回显）
// 生成随机盐值
// 计算密码哈希
// 将用户信息添加到passwd文件
```

#### login命令
用户登录认证：
```c
// 提示输入用户名和密码
// 从passwd文件查找用户
// 验证密码
// 设置进程UID和GID
// 执行shell
```

#### whoami命令
显示当前用户名：
```c
// 获取当前进程UID
// 从passwd文件查找对应用户名
// 输出用户名
```

## 2. 进程身份

### 2.1 进程结构

在`kernel/proc.h`的`struct proc`中添加了用户身份字段：

```c
struct proc {
  // ... 其他字段 ...
  uint uid;  // 用户ID
  uint gid;  // 组ID
  // ... 其他字段 ...
};
```

### 2.2 系统调用

实现了以下系统调用用于进程身份管理：

```c
// 获取当前进程UID
uint64 sys_getuid(void);

// 设置当前进程UID
uint64 sys_setuid(void);

// 获取当前进程GID
uint64 sys_getgid(void);

// 设置当前进程GID
uint64 sys_setgid(void);
```

### 2.3 权限检查逻辑

修改了`sys_setuid`和`sys_setgid`函数的权限检查逻辑：

```c
// sys_setuid函数
if(uid != 0 && p->uid != 0 && p->uid != uid) {
    return -1;  // 拒绝权限不足的请求
}
p->uid = uid;

// sys_setgid函数
if(gid != 0 && p->uid != 0 && p->gid != gid) {
    return -1;  // 拒绝权限不足的请求
}
p->gid = gid;
```

这个修改允许任何用户切换到root用户（UID=0或GID=0），同时保持其他权限检查不变。

## 3. 文件权限控制

### 3.1 文件所有权

在`kernel/fs.h`的`struct dinode`中添加了文件所有权字段：

```c
struct dinode {
  // ... 其他字段 ...
  short uid;  // 文件所有者UID
  short gid;  // 文件所有者GID
  // ... 其他字段 ...
};
```

### 3.2 访问控制列表

实现了12位的访问控制列表（ACL），格式为Unix风格的权限位：

```
权限位格式：[setuid][setgid][sticky][rwx][rwx][rwx]
           |     |      |     |    |    |
           |     |      |     |    |    +-- 其他用户权限
           |     |      |     |    +------- 用户组权限
           |     |      |     +------------ 文件所有者权限
           |     |      +------------------ 粘着位
           |     +------------------------- 设置组ID位
           +------------------------------- 设置用户ID位
```

### 3.3 权限检查函数

实现了内核函数`access`用于检查文件访问权限：

```c
int access(struct inode *ip, int perm) {
  // 检查读权限
  if(perm & 4) {
    if(p->uid == 0) return 0;  // root用户总是有权限
    if(p->uid == ip->uid && (ip->mode & 0400)) return 0;  // 所有者读权限
    if(p->gid == ip->gid && (ip->mode & 0040)) return 0;  // 组读权限
    if(ip->mode & 0004) return 0;  // 其他用户读权限
    return -1;
  }
  
  // 检查写权限
  if(perm & 2) {
    if(p->uid == 0) return 0;  // root用户总是有权限
    if(p->uid == ip->uid && (ip->mode & 0200)) return 0;  // 所有者写权限
    if(p->gid == ip->gid && (ip->mode & 0020)) return 0;  // 组写权限
    if(ip->mode & 0002) return 0;  // 其他用户写权限
    return -1;
  }
  
  // 检查执行权限
  if(perm & 1) {
    if(p->uid == 0) return 0;  // root用户总是有权限
    if(p->uid == ip->uid && (ip->mode & 0100)) return 0;  // 所有者执行权限
    if(p->gid == ip->gid && (ip->mode & 0010)) return 0;  // 组执行权限
    if(ip->mode & 0001) return 0;  // 其他用户执行权限
    return -1;
  }
  
  return 0;
}
```

### 3.4 文件操作权限检查

修改了以下内核函数，添加了权限检查：

- `sys_open`: 检查文件打开权限
- `create`: 检查文件创建权限
- `sys_exec`: 检查文件执行权限
- `sys_link`: 检查硬链接创建权限
- `sys_unlink`: 检查文件删除权限
- `sys_chdir`: 检查目录进入权限

## 4. 文件权限命令

### 4.1 chmod命令

修改文件权限：

```c
int main(int argc, char *argv[]) {
  if(argc < 3) {
    printf("Usage: chmod mode file...\n");
    exit(1);
  }
  
  int mode = atoi(argv[1]);
  for(int i = 2; i < argc; i++) {
    if(chmod(argv[i], mode) < 0) {
      printf("chmod: %s failed to change mode\n", argv[i]);
    }
  }
  exit(0);
}
```

### 4.2 chown命令

修改文件所有者：

```c
int main(int argc, char *argv[]) {
  if(argc < 3) {
    printf("Usage: chown uid file...\n");
    exit(1);
  }
  
  int uid = atoi(argv[1]);
  for(int i = 2; i < argc; i++) {
    if(chown(argv[i], uid) < 0) {
      printf("chown: %s failed to change owner\n", argv[i]);
    }
  }
  exit(0);
}
```

## 5. 默认文件权限

### 5.1 文件创建权限

新创建的文件默认权限为0666（所有者、组和其他用户都有读写权限），但受umask影响。

### 5.2 目录创建权限

新创建的目录默认权限为0777（所有者、组和其他用户都有读、写和执行权限），但受umask影响。

### 5.3 文件所有权继承

新创建的文件和目录继承创建进程的UID和GID：

```c
// 在create函数中
ip->uid = p->uid;
ip->gid = p->gid;
```

## 6. Set-UID机制

实现了Set-UID机制，允许进程在执行时获得文件所有者的权限：

```c
// 在exec函数中
if(ip->mode & 04000) {  // 检查set-uid位
  p->suid = ip->uid;    // 保存set-uid
}
```

## 7. 测试与验证

### 7.1 自动化测试

实现了`perms_test.c`程序，自动测试以下功能：
- 用户身份验证
- 文件权限控制
- 目录权限控制
- 用户切换功能
- 密码文件操作

### 7.2 手动测试

提供了详细的手动测试指南，包括：
- 用户登录和切换
- 文件权限设置和验证
- 文件所有权修改
- 目录权限控制
- 用户创建和管理

## 8. 安全考虑

### 8.1 密码安全

- 使用哈希加盐的方式存储密码
- 密码输入时不回显
- 密码文件权限受限

### 8.2 权限控制

- 实现了最小权限原则
- 普通用户默认权限受限
- root用户拥有完全权限

### 8.3 特殊权限

- 实现了Set-UID机制
- 实现了Set-GID机制
- 实现了粘着位机制

## 9. 未来改进

### 9.1 密码策略

- 实现密码强度检查
- 实现密码过期机制
- 实现密码历史记录

### 9.2 访问控制

- 实现访问控制列表（ACL）
- 实现角色基础访问控制（RBAC）
- 实现强制访问控制（MAC）

### 9.3 审计

- 实现系统调用审计
- 实现文件访问审计
- 实现用户操作审计

## 10. 总结

本文档描述了在xv6操作系统中实现的访问控制机制，包括用户身份验证、文件权限控制和用户权限管理等功能。这些实现遵循Unix风格的权限模型，同时根据项目需求进行了适当的调整。

通过这些实现，xv6系统现在具备了基本的访问控制能力，可以有效地保护系统资源，防止未授权访问。同时，这些实现也为未来的扩展和改进提供了基础。