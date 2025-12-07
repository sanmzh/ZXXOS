#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define N  1000

void
print(const char *s)
{
  write(1, s, strlen(s));
}
void test_with_cow(void) {
    int *ptr = malloc(sizeof(int));
    *ptr = 100;  // 写入内存
    
    int pid = fork();
    if(pid == 0) {
        // 子进程尝试写入 -> 触发COW
        *ptr = 200;  // 这里可能失败！
        printf("子进程写入: %d\n", *ptr);
        exit(0);
    } else {
        wait(0);
        printf("父进程值: %d\n", *ptr);  // 应该还是100
    }
}
void
forktest(void)
{
  int n, pid;

  print("fork test\n");

  for(n=0; n<N; n++){
    pid = fork();
    if(pid < 0)
      break;
    if(pid == 0)
      exit(0);
  }

  if(n == N){
    print("fork claimed to work N times!\n");
    exit(1);
  }

  for(; n > 0; n--){
    if(wait(0) < 0){
      print("wait stopped early\n");
      exit(1);
    }
  }

  if(wait(0) != -1){
    print("wait got too many\n");
    exit(1);
  }

  print("fork test OK\n");
  
  
  
}

int
main(void)
{
  test_with_cow();
  forktest();
  exit(0);
}
