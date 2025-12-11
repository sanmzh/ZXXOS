#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"

#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_getuid(void)
{
  return myproc()->uid;
}

uint64
sys_getgid(void)
{
  return myproc()->gid;
}

uint64
sys_setuid(void)
{
  int uid;
  if(argint(0, &uid) < 0)
    return -1;
  
  // 只有root用户(UID=0)可以设置UID为任何值
  if(myproc()->uid != 0)
    return -1;
    
  myproc()->uid = uid;
  return 0;
}

uint64
sys_setgid(void)
{
  int gid;
  if(argint(0, &gid) < 0)
    return -1;
  
  // 只有root用户(UID=0)可以设置GID为任何值
  if(myproc()->uid != 0)
    return -1;
    
  myproc()->gid = gid;
  return 0;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    if(addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;
  
  // 打印调用栈轨迹
  backtrace();

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  #ifdef SCHEDULER_MLFQ
  // MLFQ 算法需要记录每次 sleep 的休眠 tick 数累积，从而判断 I/O 密集还是 CPU 密集
  int slept = 0;
  #endif

  if (argint(0, &n) < 0) 
    return -1;
  
  // 将秒转换为tick，系统每秒大约有10个tick（每0.1秒一个tick）
  n = n * 10;

  acquire(&tickslock);
  ticks0 = ticks;

  while(ticks - ticks0 < n){
    if(killed(myproc())){

      #ifdef SCHEDULER_MLFQ
      slept = ticks - ticks0;
      #endif

      release(&tickslock);

      #ifdef SCHEDULER_MLFQ
      if (slept > 0) {
        mlfq_account_sleep(myproc(), slept);
      }
      #endif

      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);

  #ifdef SCHEDULER_MLFQ
  slept = ticks - ticks0;
  if (slept > 0) {
    mlfq_account_sleep(myproc(), slept);
  }
  #endif
  
  return 0;
}

int
sys_kpgtbl(void)          // LAB_PGTBL
{
  struct proc *p;

  p = myproc();
  vmprint(p->pagetable);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
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

  argint(0, &mask);               // 通过读取进程的trapframe，获得 mask 参数    

  myproc()->trace_mask = mask;    // 将 mask 参数保存到当前进程的 trace_mask 字段中
  return 0;
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

// LAB_LOCK
uint64
sys_cpupin(void)
{
  struct proc *p = myproc();
  int cpu;

  argint(0, &cpu);
  if (cpu < 0 || cpu >= NCPU)
    return -1;
  acquire(&p->lock);
  p->pincpu = &cpus[cpu];
  release(&p->lock);
  return 0;
}
// END LAB_LOCK

#ifdef SCHEDULER_RR
/**
 * @brief RR 算法所需内核函数，设置当前进程的时间片
 * @param timeslice 新的时间片长度
 * @return 0 表示系统调用成功返回，-1 表示参数解析失败
 */
uint64 sys_set_timeslice(void) {
  int timeslice;
  if (argint(0, &timeslice) < 0) {
    return -1;
  }
  struct proc* p = myproc();
  // 合法性校验
  if (timeslice < 1) {
    return -1;
  }
  acquire(&p->lock);
  p->timeslice = timeslice;
  p->slice_remaining = timeslice;
  release(&p->lock);
  return 0;
}
#endif

// kernel/sysproc.c
#if defined(SCHEDULER_PRIORITY) || defined(SCHEDULER_MLFQ)
/**
 * @brief 优先级 / MLFQ 调度算法所需内核函数，设置当前进程的优先级
 * @param priority 新的优先级
 * @return 0 表示系统调用成功返回，-1 表示参数解析失败
 */
uint64 sys_set_priority(void) {
  int priority;
  if (argint(0, &priority) < 0) {
    return -1;
  }
  struct proc* p = myproc();

  #ifdef SCHEDULER_PRIORITY
  // 优先级调度：拒绝负值优先级
  if (priority < 0) {
    return -1;
  }
  acquire(&p->lock);
  p->priority = priority;
  release(&p->lock);
  #endif

  #ifdef SCHEDULER_MLFQ
  acquire(&p->lock);
  // MLFQ：裁剪优先级到合法区间，并更新动态优先级、重置统计数据
  p->priority = mlfq_clamp_priority(priority);
  p->base_priority = p->priority;
  p->ticks_used = 0;
  p->eval_ticks = 0;
  p->cpu_ticks = 0;
  p->sleep_ticks = 0;
  release(&p->lock);
  #endif

  return 0;
}

/**
 * @brief 优先级 / MLFQ 算法所需内核函数，实现 get_priority 系统调用，获取当前进程的优先级。
 * @return 当前进程的优先级（占位实现固定返回0）
 */
uint64 sys_get_priority(void) {
  struct proc* p = myproc();
  acquire(&p->lock);
  int priority = p->priority;
  release(&p->lock);
  return priority;
}
#endif