#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

int
main(int argc, char *argv[])
{
  int uid, gid;
  char *path;
  
  if(argc < 3 || argc > 4){
    fprintf(2, "Usage: chown owner[:group] file...\n");
    fprintf(2, "       chown uid gid file...\n");
    exit(1);
  }
  
  // 解析参数
  if(argc == 3) {
    // 格式: chown owner[:group] file
    path = argv[2];
    
    // 解析owner[:group]格式
    char *colon = strchr(argv[1], ':');
    if(colon) {
      // owner:group格式
      *colon = '\0'; // 分割字符串
      uid = atoi(argv[1]);
      gid = atoi(colon + 1);
    } else {
      // 只有owner，group不变
      uid = atoi(argv[1]);
      gid = -1; // 表示不改变组
    }
  } else {
    // 格式: chown uid gid file
    uid = atoi(argv[1]);
    gid = atoi(argv[2]);
    path = argv[3];
  }
  
  // 调用chown系统调用
  if(chown(path, uid, gid) < 0) {
    fprintf(2, "chown: failed to change ownership of %s\n", path);
    exit(1);
  }
  
  exit(0);
}