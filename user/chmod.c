#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

// 将字符串解析为八进制数
int parse_octal(const char *str) {
  int result = 0;
  while (*str) {
    if (*str >= '0' && *str <= '7') {
      result = result * 8 + (*str - '0');
    } else {
      return -1; // 无效的八进制数字
    }
    str++;
  }
  return result;
}

int
main(int argc, char *argv[])
{
  int mode;
  char *path;
  
  if(argc < 3){
    fprintf(2, "Usage: chmod mode file...\n");
    exit(1);
  }
  
  // 解析权限模式为八进制数
  mode = parse_octal(argv[1]);
  if(mode < 0) {
    fprintf(2, "chmod: invalid mode\n");
    exit(1);
  }
  
  // 对每个文件参数执行chmod
  for(int i = 2; i < argc; i++) {
    path = argv[i];
    
    // 调用chmod系统调用
    if(chmod(path, mode) < 0) {
      fprintf(2, "chmod: failed to change mode of %s\n", path);
      exit(1);
    }
  }
  
  exit(0);
}