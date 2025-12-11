#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/pwd.h"
#include "user/hash.h"

// 获取用户输入，不显示在终端上
int get_password(char *buf, int size) {
  int i = 0;
  char c;
  
  printf("Password: ");
  
  // 简单实现：读取输入但不显示
  while (i < size - 1 && read(0, &c, 1) == 1) {
    if (c == '\n' || c == '\r') {
      break;
    }
    buf[i++] = c;
  }
  
  buf[i] = '\0';
  printf("\n");
  
  return i;
}

// 获取普通用户输入
int get_input(char *buf, int size) {
  int i = 0;
  char c;
  
  while (i < size - 1 && read(0, &c, 1) == 1) {
    if (c == '\n' || c == '\r') {
      break;
    }
    buf[i++] = c;
  }
  
  buf[i] = '\0';
  
  return i;
}

int main(int argc, char *argv[]) {
  char username[32];
  char password[32];
  char password_confirm[32];
  char hashed_password[64];
  struct passwd pw;
  unsigned int next_uid = 1000;  // 普通用户从1000开始
  
  printf("=== 用户添加程序 ===\n");
  
  // 获取用户名
  printf("请输入用户名: ");
  if (get_input(username, sizeof(username)) == 0) {
    printf("错误: 用户名不能为空\n");
    exit(1);
  }
  
  // 检查用户名是否已存在
  struct passwd *existing = getpwnam(username);
  if (existing != 0) {
    printf("错误: 用户名 '%s' 已存在\n", username);
    exit(1);
  }
  
  // 获取密码
  if (get_password(password, sizeof(password)) == 0) {
    printf("错误: 密码不能为空\n");
    exit(1);
  }
  
  // 确认密码
  printf("确认密码: ");
  if (get_password(password_confirm, sizeof(password_confirm)) == 0) {
    printf("错误: 确认密码不能为空\n");
    exit(1);
  }
  
  // 检查两次输入的密码是否一致
  if (strcmp(password, password_confirm) != 0) {
    printf("错误: 两次输入的密码不一致\n");
    exit(1);
  }
  
  // 生成加盐哈希密码
  generate_hashed_password(password, hashed_password);
  
  // 确定下一个可用的UID
  setpwent();
  while ((existing = getpwent()) != 0) {
    if (existing->pw_uid >= next_uid) {
      next_uid = existing->pw_uid + 1;
    }
  }
  endpwent();
  
  // 设置passwd结构体
  pw.pw_name = username;
  pw.pw_passwd = hashed_password;
  pw.pw_uid = next_uid;
  pw.pw_gid = next_uid;  // 默认GID与UID相同
  pw.pw_gecos = "";
  pw.pw_dir = "/";       // 简化实现，所有用户主目录都是根目录
  pw.pw_shell = "/sh";   // 默认shell
  
  // 写入passwd文件
  if (putpwent(&pw) < 0) {
    printf("错误: 无法写入密码文件\n");
    exit(1);
  }
  
  printf("用户 '%s' 创建成功，UID: %d\n", username, next_uid);
  
  exit(0);
}