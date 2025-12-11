// init: The initial user-level program

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#ifdef riscv
char *argv[] = { "login", 0 };
#else
char *argv[] = { "sh", 0 };
#endif

int
main(void)
{
  int pid, wpid;

  if(open("console", O_RDWR) < 0){
    mknod("console", CONSOLE, 0);
    #ifdef riscv
    mknod("statistics", STATS, 0);
    #endif
    open("console", O_RDWR);
   
  }
  dup(0);  // stdout
  dup(0);  // stderr

  for(;;){
    #ifdef riscv
    printf("init: riscv login\n");
    #else
    printf("init: starting sh\n");
    #endif
    pid = fork();
    if(pid < 0){
      printf("init: fork failed\n");
      exit(1);
    }
    if(pid == 0){
      #ifdef riscv
      exec("login", argv);
      printf("init: riscv exec login failed\n");
      #else
      exec("sh", argv);
      printf("init: exec sh failed\n");
      #endif
      exit(1);
    }

    for(;;){
      // this call to wait() returns if the shell exits,
      // or if a parentless process exits.
      wpid = wait((int *) 0);
      if(wpid == pid){
        // the shell exited; restart it.
        break;
      } else if(wpid < 0){
        printf("init: wait returned an error\n");
        exit(1);
      } else {
        // it was a parentless process; do nothing.
      }
    }
  }
}
