#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/pwd.h"

static int passwd_fd = -1;

// 重置passwd文件偏移量到开始位置
void setpwent(void) {
  if (passwd_fd >= 0) {
    close(passwd_fd);
  }
  passwd_fd = open(PASSWD_PATH, 0);
}

// 关闭passwd文件
void endpwent(void) {
  if (passwd_fd >= 0) {
    close(passwd_fd);
    passwd_fd = -1;
  }
}

// 读取passwd文件的下一个条目
struct passwd *getpwent(void) {
  static struct passwd pw;
  static char buf[512];
  
  if (passwd_fd < 0) {
    setpwent();
    if (passwd_fd < 0) {
      return 0;
    }
  }
  
  // 读取一行
  int i = 0;
  char c;
  while (i < sizeof(buf) - 1 && read(passwd_fd, &c, 1) == 1) {
    if (c == '\n') {
      break;
    }
    buf[i++] = c;
  }
  
  if (i == 0) {
    return 0;  // 文件结束或错误
  }
  
  buf[i] = '\0';
  
  // 解析字段
  char *fields[7];
  int field_count = 0;
  char *p = buf;
  fields[field_count++] = p;
  
  while (*p && field_count < 7) {
    if (*p == ':') {
      *p = '\0';
      p++;
      fields[field_count++] = p;
    } else {
      p++;
    }
  }
  
  // 至少需要用户名、密码、uid、gid
  if (field_count < 4) {
    return 0;
  }
  
  pw.pw_name = fields[0];
  pw.pw_passwd = fields[1];
  pw.pw_uid = atoi(fields[2]);
  pw.pw_gid = atoi(fields[3]);
  pw.pw_gecos = field_count > 4 ? fields[4] : "";
  pw.pw_dir = field_count > 5 ? fields[5] : "/";
  pw.pw_shell = field_count > 6 ? fields[6] : "/sh";
  
  return &pw;
}

// 根据用户名查找passwd条目
struct passwd *getpwnam(const char *name) {
  struct passwd *pw;
  
  setpwent();
  while ((pw = getpwent()) != 0) {
    if (strcmp(pw->pw_name, name) == 0) {
      return pw;
    }
  }
  
  return 0;
}

// 根据用户ID查找passwd条目
struct passwd *getpwuid(unsigned int uid) {
  struct passwd *pw;
  
  setpwent();
  while ((pw = getpwent()) != 0) {
    if (pw->pw_uid == uid) {
      return pw;
    }
  }
  
  return 0;
}

// 向passwd文件写入一个条目
int putpwent(const struct passwd *pw) {
  int fd = open(PASSWD_PATH, 1 | 2 | 0x200);  // 读写模式，如果不存在则创建
  if (fd < 0) {
    return -1;
  }
  
  // 移动到文件末尾
  // 在xv6中，使用seek而不是lseek
  seek(fd, 0, 2);
  
  char buf[512];
  int len = 0;
  
  // 手动构建字符串，避免使用snprintf
  len += strlen(pw->pw_name);
  len += strlen(pw->pw_passwd);
  len += 16;  // uid和gid的空间
  len += strlen(pw->pw_gecos ? pw->pw_gecos : "");
  len += strlen(pw->pw_dir ? pw->pw_dir : "/");
  len += strlen(pw->pw_shell ? pw->pw_shell : "/sh");
  len += 6;   // 冒号和换行符
  
  if (len >= sizeof(buf)) {
    close(fd);
    return -1;
  }
  
  strcpy(buf, pw->pw_name);
  // 手动实现strcat功能
  int buf_len = strlen(buf);
  strcpy(buf + buf_len, ":");
  buf_len += 1;
  
  strcpy(buf + buf_len, pw->pw_passwd);
  buf_len += strlen(pw->pw_passwd);
  strcpy(buf + buf_len, ":");
  buf_len += 1;
  
  char uid_str[16], gid_str[16];
  itoa(pw->pw_uid, uid_str, 10);
  itoa(pw->pw_gid, gid_str, 10);
  
  strcpy(buf + buf_len, uid_str);
  buf_len += strlen(uid_str);
  strcpy(buf + buf_len, ":");
  buf_len += 1;
  
  strcpy(buf + buf_len, gid_str);
  buf_len += strlen(gid_str);
  strcpy(buf + buf_len, ":");
  buf_len += 1;
  
  strcpy(buf + buf_len, pw->pw_gecos ? pw->pw_gecos : "");
  buf_len += strlen(pw->pw_gecos ? pw->pw_gecos : "");
  strcpy(buf + buf_len, ":");
  buf_len += 1;
  
  strcpy(buf + buf_len, pw->pw_dir ? pw->pw_dir : "/");
  buf_len += strlen(pw->pw_dir ? pw->pw_dir : "/");
  strcpy(buf + buf_len, ":");
  buf_len += 1;
  
  strcpy(buf + buf_len, pw->pw_shell ? pw->pw_shell : "/sh");
  buf_len += strlen(pw->pw_shell ? pw->pw_shell : "/sh");
  strcpy(buf + buf_len, "\n");
  buf_len += 1;
  
  len = strlen(buf);
  int result = write(fd, buf, len);
  close(fd);
  
  return result == len ? 0 : -1;
}