#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "syscall.h"

// sys_shmget: 创建或获取共享内存标识符
uint64
sys_shmget(void)
{
  int key, size, shmflg;

  argint(0, &key);
  argint(1, &size);
  argint(2, &shmflg);

  return shmget(key, size, shmflg);
}

// sys_shmat: 将共享内存附加到进程地址空间
uint64
sys_shmat(void)
{
  int shmid;
  uint64 addr;
  int shmflg;

  argint(0, &shmid);
  argaddr(1, &addr);
  argint(2, &shmflg);

  return (uint64)shmat(shmid, (const void*)addr, shmflg);
}

// sys_shmdt: 从进程地址空间分离共享内存
uint64
sys_shmdt(void)
{
  uint64 addr;

  argaddr(0, &addr);

  return shmdt((const void*)addr);
}

// sys_shmctl: 共享内存控制操作
uint64
sys_shmctl(void)
{
  int shmid, cmd;
  uint64 buf;

  argint(0, &shmid);
  argint(1, &cmd);
  argaddr(2, &buf);

  return shmctl(shmid, cmd, (void*)buf);
}
