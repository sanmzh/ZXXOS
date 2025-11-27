
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "syscall.h"
#include "ipc.h"

// sys_semget: 创建或获取信号量集标识符
uint64
sys_semget(void)
{
  int key, nsems, semflg;

  if(argint(0, &key) < 0)
    return -1;
  if(argint(1, &nsems) < 0)
    return -1;
  if(argint(2, &semflg) < 0)
    return -1;

  return semget(key, nsems, semflg);
}

// sys_semop: 信号量操作
uint64
sys_semop(void)
{
  int semid;
  uint64 sops_ptr;
  unsigned nsops;

  if(argint(0, &semid) < 0)
    return -1;
  if(argaddr(1, &sops_ptr) < 0)
    return -1;
  if(argint(2, (int*)&nsops) < 0)
    return -1;

  return semop(semid, (struct sembuf*)sops_ptr, nsops);
}

// sys_semctl: 信号量控制操作
uint64
sys_semctl(void)
{
  int semid, semnum, cmd;
  uint64 arg_ptr;

  if(argint(0, &semid) < 0)
    return -1;
  if(argint(1, &semnum) < 0)
    return -1;
  if(argint(2, &cmd) < 0)
    return -1;
  if(argaddr(3, &arg_ptr) < 0)
    return -1;

  return semctl(semid, semnum, cmd, (void*)arg_ptr);
}
