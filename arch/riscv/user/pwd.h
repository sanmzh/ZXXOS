#define PASSWD_PATH "/password.txt"

struct passwd {
  char *pw_name;   // 用户名
  char *pw_passwd;  // 密码（哈希值和盐）
  unsigned int pw_uid;      // 用户ID
  unsigned int pw_gid;      // 组ID
  char *pw_gecos;   // 用户信息字段
  char *pw_dir;     // 主目录
  char *pw_shell;   // 默认shell
};

// taken from man://pwd.h (POSIX)
struct passwd *getpwent(void);
void setpwent(void);
void endpwent(void);
struct passwd *getpwnam(const char *);
// int getpwnam_r(const char *, struct passwd *, char *, size_t, struct passwd **);
struct passwd *getpwuid(unsigned int);
// int getpwuid_r(uid_t, struct passwd *, char *, size_t, struct passwd **);
int putpwent(const struct passwd *);