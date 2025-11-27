#include "types.h"
#include "loongarch.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

// 获取系统信息
uint64
sys_sysinfo(void)
{
  struct sysinfo info;
  freebytes(&info.freemem);
  proccount(&info.nproc);

  // 获取虚拟地址
  uint64 dstva;
  argaddr(0, &dstva);

  // 将 info 结构体从内核空间复制到用户空间
  if(copyout(myproc()->pagetable, dstva, (char *)&info, sizeof(info)) < 0)
    return -1;

  return 0;
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0){
    return -1;}
  addr = myproc()->sz;
  if(growproc(n) < 0){
    return -1;}
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// 当前进程的系统调用跟踪掩码
uint64
sys_trace(void)
{
  int mask;
  printf("1");
  argint(0, &mask);               // 通过读取进程的trapframe，获得 mask 参数    
  printf("2");
  myproc()->trace_mask = mask;    // 将 mask 参数保存到当前进程的 trace_mask 字段中
  return 0;
}
