#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/pwd.h"

int main(int argc, char *argv[]) {
  unsigned int uid = getuid();
  // printf("调试: whoami - getuid() 返回 %d\n", uid);
  struct passwd *pw = getpwuid(uid);
  
  if (pw != 0) {
    printf("%s\n", pw->pw_name);
  } else {
    printf("uid %d\n", uid);
  }
  
  exit(0);
}