#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "../arch/riscv/user/pwd.h"

// 测试用户身份验证功能
void test_user_authentication() {
  printf("=== 测试用户身份验证功能 ===\n");
  
  // 获取当前用户信息
  uint uid = getuid();
  uint gid = getgid();
  uint original_uid = uid; // 保存原始UID
  printf("当前用户ID: %d, 组ID: %d\n", uid, gid);
  
  // 尝试获取当前用户名
  struct passwd *pw = getpwuid(uid);
  if(pw != 0) {
    printf("当前用户名: %s\n", pw->pw_name);
    printf("用户信息: UID=%d, GID=%d\n", pw->pw_uid, pw->pw_gid);
  } else {
    printf("无法获取当前用户信息\n");
  }
  
  // 测试root用户特权
  if(uid == 0) {
    printf("当前是root用户，具有管理员权限\n");
    
    // 测试设置UID和GID
    printf("\n测试设置UID和GID:\n");
    if(setuid(1000) == 0) {
      printf("成功将UID设置为1000\n");
      printf("设置后UID: %d\n", getuid());
      
      // 恢复root权限
      if(setuid(original_uid) == 0) {
        printf("成功恢复UID为%d: %d\n", original_uid, getuid());
      } else {
        printf("恢复UID为%d失败，当前UID: %d\n", original_uid, getuid());
      }
    } else {
      printf("设置UID失败\n");
    }
  } else {
    printf("当前是普通用户，UID: %d\n", uid);
    
    // 测试普通用户尝试设置UID为root
    printf("\n测试普通用户尝试设置UID为root:\n");
    if(setuid(0) == 0) {
      printf("意外成功将UID设置为0（这不应该发生）\n");
      setuid(uid); // 恢复原UID
    } else {
      printf("正确拒绝了普通用户设置UID为root的请求\n");
    }
  }
}

// 测试文件权限功能
void test_file_permissions() {
  printf("\n=== 测试文件权限功能 ===\n");
  
  // 创建测试文件
  int fd = open("testfile", O_CREATE | O_WRONLY);
  if(fd < 0) {
    printf("创建测试文件失败\n");
    return;
  }
  write(fd, "test content", 12);
  close(fd);
  
  // 获取当前用户ID
  uint uid = getuid();
  uint gid = getgid();
  printf("当前用户ID: %d, 组ID: %d\n", uid, gid);
  
  // 1. 测试修改文件权限
  printf("\n1. 测试chmod命令:\n");
  
  // 设置文件权限为只有所有者可读写 (0600)
  if(chmod("testfile", 0600) == 0) {
    printf("成功将文件权限设置为0600\n");
  } else {
    printf("设置文件权限为0600失败\n");
  }
  
  // 设置文件权限为所有用户可读写 (0666)
  if(chmod("testfile", 0666) == 0) {
    printf("成功将文件权限设置为0666\n");
  } else {
    printf("设置文件权限为0666失败\n");
  }
  
  // 设置文件权限为所有者可读写，组和其他用户只读 (0644)
  if(chmod("testfile", 0644) == 0) {
    printf("成功将文件权限设置为0644\n");
  } else {
    printf("设置文件权限为0644失败\n");
  }
  
  // 2. 测试修改文件所有权
  printf("\n2. 测试chown命令:\n");
  
  // 只有root用户才能修改文件所有权
  if(uid == 0) {
    // 将文件所有权设置为用户1000, 组1000
    if(chown("testfile", 1000, 1000) == 0) {
      printf("成功将文件所有权设置为用户1000, 组1000\n");
    } else {
      printf("设置文件所有权失败\n");
    }
    
    // 恢复文件所有权为root
    if(chown("testfile", 0, 0) == 0) {
      printf("成功将文件所有权恢复为root\n");
    } else {
      printf("恢复文件所有权失败\n");
    }
  } else {
    printf("非root用户，跳过chown测试\n");
    
    // 尝试修改文件所有权（应该失败）
    if(chown("testfile", uid, gid) == 0) {
      printf("意外成功修改了文件所有权（这不应该发生）\n");
    } else {
      printf("正确拒绝了非root用户修改文件所有权的请求\n");
    }
  }
  
  // 3. 测试访问权限
  printf("\n3. 测试文件访问权限:\n");
  
  // 设置文件权限为只有所有者可读写 (0600)
  chmod("testfile", 0600);
  
  // 尝试读取文件
  fd = open("testfile", O_RDONLY);
  if(fd >= 0) {
    printf("成功打开文件进行读取\n");
    char buf[20];
    int n = read(fd, buf, sizeof(buf)-1);
    if(n > 0) {
      buf[n] = '\0';
      printf("文件内容: %s\n", buf);
    }
    close(fd);
  } else {
    printf("无法打开文件进行读取\n");
  }
  
  // 尝试写入文件
  fd = open("testfile", O_WRONLY);
  if(fd >= 0) {
    printf("成功打开文件进行写入\n");
    write(fd, "new content", 12);
    close(fd);
  } else {
    printf("无法打开文件进行写入\n");
  }
  
  // 设置文件权限为所有用户只读 (0444)
  chmod("testfile", 0444);
  
  // 尝试写入只读文件
  fd = open("testfile", O_WRONLY);
  if(fd >= 0) {
    printf("意外成功打开只读文件进行写入（这不应该发生）\n");
    close(fd);
  } else {
    printf("正确拒绝了对只读文件的写入请求\n");
  }
  
  // 清理测试文件
  unlink("testfile");
  printf("\n测试完成，已清理测试文件\n");
}

// 测试目录权限
void test_directory_permissions() {
  printf("\n=== 测试目录权限 ===\n");
  
  // 创建测试目录
  if(mkdir("testdir") < 0) {
    printf("创建测试目录失败\n");
    return;
  }
  
  // 设置目录权限为只有所有者可读写执行 (0700)
  if(chmod("testdir", 0700) == 0) {
    printf("成功将目录权限设置为0700\n");
  } else {
    printf("设置目录权限为0700失败\n");
  }
  
  // 尝试在目录中创建文件
  char path[20];
  strcpy(path, "testdir/file");
  int fd = open(path, O_CREATE | O_WRONLY);
  if(fd >= 0) {
    printf("成功在受限目录中创建文件\n");
    close(fd);
    unlink(path);
  } else {
    printf("无法在受限目录中创建文件\n");
  }
  
  // 设置目录权限为所有用户可读写执行 (0777)
  if(chmod("testdir", 0777) == 0) {
    printf("成功将目录权限设置为0777\n");
  } else {
    printf("设置目录权限为0777失败\n");
  }
  
  // 尝试在目录中创建文件
  fd = open(path, O_CREATE | O_WRONLY);
  if(fd >= 0) {
    printf("成功在开放目录中创建文件\n");
    close(fd);
    unlink(path);
  } else {
    printf("无法在开放目录中创建文件\n");
  }
  
  // 清理测试目录
  unlink("testdir");
  printf("测试完成，已清理测试目录\n");
}

// 测试用户切换
void test_user_switching() {
  printf("\n=== 测试用户切换功能 ===\n");
  
  uint current_uid = getuid();
  uint original_uid = current_uid; // 保存原始UID
  printf("当前用户ID: %d\n", current_uid);
  
  // 如果是root用户，测试切换到其他用户
  if(current_uid == 0) {
    printf("当前是root用户，测试切换到普通用户:\n");
    
    // 切换到用户1000
    if(setuid(1000) == 0) {
      printf("成功切换到用户1000\n");
      printf("切换后UID: %d\n", getuid());
      
      // 尝试执行需要root权限的操作（现在应该成功）
      if(setuid(0) == 0) {
        printf("成功从普通用户切换回root\n");
        printf("切换后UID: %d\n", getuid());
      } else {
        printf("从普通用户切换回root失败\n");
      }
      
      // 恢复root权限
      setuid(0);
      printf("恢复root权限: %d\n", getuid());
    } else {
      printf("切换到用户1000失败\n");
    }
  } else {
    printf("当前是普通用户，测试切换到其他用户:\n");
    
    // 尝试切换到其他普通用户
    if(setuid(1001) == 0) {
      printf("成功切换到用户1001\n");
      printf("切换后UID: %d\n", getuid());
      
      // 恢复原用户
      setuid(current_uid);
      printf("恢复原用户权限: %d\n", getuid());
    } else {
      printf("切换到用户1001失败\n");
    }
    
    // 尝试切换到root（现在应该成功）
    if(setuid(0) == 0) {
      printf("成功切换到root用户\n");
      printf("切换后UID: %d\n", getuid());
      
      // 恢复原用户
      setuid(current_uid);
      printf("恢复原用户权限: %d\n", getuid());
    } else {
      printf("切换到root用户失败\n");
    }
  }
  
  // 确保最终恢复到原始用户
  if(getuid() != original_uid) {
    setuid(original_uid);
    printf("恢复到原始用户权限: %d\n", getuid());
  }
}

// 测试密码文件操作
void test_passwd_operations() {
  printf("\n=== 测试密码文件操作 ===\n");
  
  // 测试获取所有用户信息
  printf("系统中的用户列表:\n");
  setpwent(); // 重置到文件开头
  
  struct passwd *pw;
  int user_count = 0;
  while((pw = getpwent()) != 0) {
    printf("用户名: %s, UID: %d, GID: %d\n", pw->pw_name, pw->pw_uid, pw->pw_gid);
    user_count++;
  }
  
  if(user_count == 0) {
    printf("没有找到任何用户\n");
  } else {
    printf("共找到 %d 个用户\n", user_count);
  }
  
  endpwent(); // 关闭文件
  
  // 测试按用户名查找用户
  printf("\n测试按用户名查找用户:\n");
  pw = getpwnam("root");
  if(pw != 0) {
    printf("找到root用户: UID=%d, GID=%d\n", pw->pw_uid, pw->pw_gid);
  } else {
    printf("未找到root用户\n");
  }
  
  // 测试按UID查找用户
  printf("\n测试按UID查找用户:\n");
  pw = getpwuid(0);
  if(pw != 0) {
    printf("找到UID=0的用户: %s\n", pw->pw_name);
  } else {
    printf("未找到UID=0的用户\n");
  }
}

int main() {
  printf("开始全面权限和身份验证测试\n");
  
  test_user_authentication();
  test_file_permissions();
  test_directory_permissions();
  test_user_switching();
  test_passwd_operations();
  
  printf("\n所有测试完成\n");
  exit(0);
}