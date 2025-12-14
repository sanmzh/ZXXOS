#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#ifdef riscv
#include "kernel/fcntl.h"
#endif

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  #ifdef riscv
  buf[sizeof(buf)-1] = '\0';
  #endif
  return buf;
}

#ifdef riscv
// 格式化权限位为可读形式
void
format_permissions(int mode, char* perms) {
  // 文件类型 - 使用stat结构中的type字段而不是mode中的位
  // 这里我们只处理权限位，文件类型将在调用处处理
  perms[0] = '-'; // 默认为普通文件
  
  // 所有者权限
  perms[1] = (mode & 1<<8) ? 'r' : '-';  // 所有者读权限 (0400)
  perms[2] = (mode & 1<<7) ? 'w' : '-';  // 所有者写权限 (0200)
  perms[3] = (mode & 1<<6) ? 'x' : '-';  // 所有者执行权限 (0100)
  
  // 组权限
  perms[4] = (mode & 1<<5) ? 'r' : '-';  // 组读权限 (0040)
  perms[5] = (mode & 1<<4) ? 'w' : '-';  // 组写权限 (0020)
  perms[6] = (mode & 1<<3) ? 'x' : '-';  // 组执行权限 (0010)
  
  // 其他用户权限
  perms[7] = (mode & 1<<2) ? 'r' : '-';  // 其他用户读权限 (0004)
  perms[8] = (mode & 1<<1) ? 'w' : '-';  // 其他用户写权限 (0002)
  perms[9] = (mode & 1<<0) ? 'x' : '-';  // 其他用户执行权限 (0001)
  
  perms[10] = '\0';
}
#endif

void
ls(char *path, int detailed)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  #ifdef riscv
  case T_DEVICE:
  #endif
  case T_FILE:
    #ifdef riscv
    if (detailed) {
      char perms[11];
      format_permissions(st.mode, perms);
      perms[0] = '-'; // 设置为普通文件
      printf("%s %d %d %d %s\n", perms, st.uid, st.gid, (int) st.size, fmtname(path));
    } else {
      printf("%s %d %d %d\n", fmtname(path), st.type, st.ino, (int) st.size);
    }
    #else
      printf("%s %d %d %d\n", fmtname(path), st.type, st.ino, (int) st.size);
    #endif

    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("ls: cannot stat %s\n", buf);
        continue;
      }
      #ifdef riscv
      if (detailed) {
        char perms[11];
        format_permissions(st.mode, perms);
        if (st.type == T_DIR) {
          perms[0] = 'd'; // 设置为目录
        } else {
          perms[0] = '-'; // 设置为普通文件
        }
        printf("%s %d %d %d %s\n", perms, st.uid, st.gid, (int) st.size, fmtname(buf));
      } else {
        printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, (int) st.size);
      }
      #else
        printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, (int) st.size);
      #endif
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;
  int detailed = 0;

  // 检查是否有 -l 选项
  for(i = 1; i < argc; i++) {
    if(strcmp(argv[i], "-l") == 0) {
      detailed = 1;
      // 从参数列表中移除 -l
      for(int j = i; j < argc-1; j++) {
        argv[j] = argv[j+1];
      }
      argc--;
      i--;
    }
  }

  if(argc < 2){
    ls(".", detailed);
    exit(0);
  }
  for(i=1; i<argc; i++)
    ls(argv[i], detailed);
  exit(0);
}