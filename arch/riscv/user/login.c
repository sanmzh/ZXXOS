#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/pwd.h"
#include "user/hash.h"
#include "kernel/param.h"

// 验证密码
int verify_password(const char *plain_password, const char *stored_password);

// 获取用户输入，不显示在终端上
int get_password(char *buf, int size) {
  int i = 0;
  char c;
  
  // 读取输入但不显示字符
  while (i < size - 1 && read(0, &c, 1) == 1) {
    if (c == '\n' || c == '\r') {
      break;
    }
    // 不回显字符，直接存储
    buf[i++] = c;
  }
  
  buf[i] = '\0';
  
  // 输出换行符
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

// 验证密码
int verify_password(const char *plain_password, const char *stored_password) {
  char test_hash[64];
  
  // printf("调试: 输入密码='%s', 存储密码='%s'\n", plain_password, stored_password);
  
  // 检查存储密码格式
  if (stored_password == 0 || strlen(stored_password) == 0) {
    // printf("调试: 存储密码为空\n");
    return 0;  // 密码为空
  }
  
  // 从存储的密码中提取哈希值和盐值
  // 格式应该是: $hash$salt
  char *first_dollar = strchr(stored_password, '$');
  if (first_dollar == 0) {
    // printf("调试: 没有找到盐值，使用默认盐值\n");
    // 如果没有盐值，使用默认盐值
    generate_hashed_password(plain_password, test_hash);
  } else {
    // 查找第二个$符号
    char *second_dollar = strchr(first_dollar + 1, '$');
    if (second_dollar == 0) {
      // printf("调试: 格式错误，没有第二个$\n");
      return 0;
    }
    
    // 提取盐值（第二个$后面的部分）
    int salt = atoi(second_dollar + 1);
    // printf("调试: 提取的盐值=%d\n", salt);
    generate_hashed_password_with_salt(plain_password, salt, test_hash);
  }
  
  // printf("调试: 计算得到的哈希='%s'\n", test_hash);
  
  // 比较哈希值
  int result = strcmp(test_hash, stored_password) == 0;
  // printf("调试: 比较结果=%d\n", result);
  
  return result;
}

// 创建root用户
void create_root_user() {
  struct passwd pw;
  char root_password[] = "root";  // 默认root密码
  
  printf("创建root用户...\n");
  
  // 生成root用户的哈希密码
  static char hashed_password[64];  // 使用静态变量确保内存在函数返回后仍然有效
  generate_hashed_password(root_password, hashed_password);
  
  // 设置passwd结构体
  pw.pw_name = "root";
  pw.pw_passwd = hashed_password;
  pw.pw_uid = 0;
  pw.pw_gid = 0;
  pw.pw_gecos = "Super User";
  pw.pw_dir = "/";
  pw.pw_shell = "/sh";
  
  // 写入passwd文件
  if (putpwent(&pw) < 0) {
    printf("错误: 无法创建root用户\n");
    exit(1);
  }
  
  printf("root用户创建成功，默认密码为: root\n");
}

int main(int argc, char *argv[]) {
  char username[32];
  char password[32];
  struct passwd *pw;
  int attempts = 0;
  int max_attempts = 3;
  
  // 检查passwd文件是否存在
  setpwent();
  pw = getpwent();
  if (pw == 0) {
    // passwd文件为空，创建root用户
    endpwent();
    create_root_user();
  } else {
    endpwent();
  }
  
  printf("=== xv6 登录 ===\n");
  
  while (attempts < max_attempts) {
    // 获取用户名
    printf("登录: ");
    if (get_input(username, sizeof(username)) == 0) {
      printf("错误: 用户名不能为空\n");
      attempts++;
      continue;
    }
    
    // 查找用户
    pw = getpwnam(username);
    if (pw == 0) {
      printf("错误: 用户 '%s' 不存在\n", username);
      attempts++;
      continue;
    }
    
    // 获取密码
    printf("密码: ");
    if (get_password(password, sizeof(password)) == 0) {
      printf("错误: 密码不能为空\n");
      attempts++;
      continue;
    }
    
    // 验证密码
    if (verify_password(password, pw->pw_passwd)) {
      // 密码正确，检查权限
      // printf("调试: 当前进程UID=%d, GID=%d\n", getuid(), getgid());
      // printf("调试: 尝试设置UID=%d, GID=%d\n", pw->pw_uid, pw->pw_gid);
      
      // 检查当前用户是否为root，或者是否切换到自己的账户，或者通过login重新登录
      // login命令应该允许任何用户重新登录为任何用户
      // 这里我们移除权限检查，允许任何用户通过login重新登录
      
      // 先设置GID，再设置UID，因为setgid需要当前进程UID为0
      if (setgid(pw->pw_gid) < 0) {
        printf("错误: 无法设置组ID\n");
        attempts++;
        continue;
      }
      
      if (setuid(pw->pw_uid) < 0) {
        printf("错误: 无法设置用户ID\n");
        attempts++;
        continue;
      }
      
      // printf("调试: 设置后进程UID=%d, GID=%d\n", getuid(), getgid());
      printf("登录成功! 欢迎, %s\n", username);
      
      // 执行shell
      char *shell_argv[] = {"sh", 0};
      exec("sh", shell_argv);
      
      printf("错误: 无法启动shell\n");
      exit(1);
    } else {
      printf("错误: 密码不正确\n");
      attempts++;
    }
  }
  
  printf("登录尝试次数过多，系统锁定\n");
  exit(1);
}