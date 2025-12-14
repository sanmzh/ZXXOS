#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

void test_file_permissions() {
  printf("=== 测试文件权限功能 ===\n");
  
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
  
  // 2. 测试修改文件所有权
  printf("\n2. 测试chown命令:\n");
  
  // 将文件所有权设置为当前用户
  if(chown("testfile", uid, gid) == 0) {
    printf("成功将文件所有权设置为用户%d, 组%d\n", uid, gid);
  } else {
    printf("设置文件所有权失败\n");
  }
  
  // 3. 测试访问权限
  printf("\n3. 测试文件访问权限:\n");
  
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
  
  // 清理测试文件
  unlink("testfile");
  printf("\n测试完成，已清理测试文件\n");
}

void test_permission_denied() {
  printf("\n=== 测试权限拒绝情况 ===\n");
  
  // 创建测试文件
  int fd = open("restricted", O_CREATE | O_WRONLY);
  if(fd < 0) {
    printf("创建受限文件失败\n");
    return;
  }
  write(fd, "restricted content", 18);
  close(fd);
  
  // 设置文件权限为只有所有者可读写 (0600)
  chmod("restricted", 0600);
  
  // 尝试以不同用户身份访问文件
  // 注意：在xv6中，我们无法轻易切换用户，所以这个测试可能有限
  printf("当前用户对受限文件的访问:\n");
  
  fd = open("restricted", O_RDONLY);
  if(fd >= 0) {
    printf("能够读取受限文件\n");
    close(fd);
  } else {
    printf("无法读取受限文件\n");
  }
  
  // 清理测试文件
  unlink("restricted");
  printf("测试完成，已清理受限文件\n");
}

int main() {
  printf("开始文件权限和所有权测试\n");
  
  test_file_permissions();
  test_permission_denied();
  
  printf("\n所有测试完成\n");
  exit(0);
}