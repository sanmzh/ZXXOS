#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "syscall.h"
#include "fcntl.h"
#include "ipc.h"

// System call to get a message queue
uint64
sys_msgget(void)
{
  int key, msgflg;

  if(argint(0, &key) < 0 || argint(1, &msgflg) < 0)
    return -1;

  return msgget(key, msgflg);
}

// System call to send a message
uint64
sys_msgsnd(void)
{
  int msqid, msgsz, msgflg;
  uint64 msgp;

  if(argint(0, &msqid) < 0 || argaddr(1, &msgp) < 0 || 
     argint(2, &msgsz) < 0 || argint(3, &msgflg) < 0)
    return -1;

  // 分配内核缓冲区
  struct msgbuf *mbuf = (struct msgbuf *)kalloc();
  if(!mbuf)
    return -1;

  // 从用户空间复制消息
  if(copyin(myproc()->pagetable, (char *)mbuf, msgp, sizeof(long) + msgsz) < 0) {
    kfree((char *)mbuf);
    return -1;
  }

  int ret = msgsnd(msqid, mbuf, msgsz, msgflg);
  kfree((char *)mbuf);
  return ret;
}

// System call to receive a message
uint64
sys_msgrcv(void)
{
  int msqid, msgsz, msgtyp, msgflg;
  uint64 msgp;

  if(argint(0, &msqid) < 0 || argaddr(1, &msgp) < 0 || 
     argint(2, &msgsz) < 0 || argint(3, &msgtyp) < 0 || argint(4, &msgflg) < 0)
    return -1;

  // 分配内核缓冲区
  struct msgbuf *mbuf = (struct msgbuf *)kalloc();
  if(!mbuf)
    return -1;

  int ret = msgrcv(msqid, mbuf, msgsz, msgtyp, msgflg);
  
  // 如果成功接收消息，复制到用户空间
  if(ret >= 0) {
    if(copyout(myproc()->pagetable, msgp, (char *)mbuf, sizeof(long) + ret) < 0) {
      kfree((char *)mbuf);
      return -1;
    }
  }
  
  kfree((char *)mbuf);
  return ret;
}

// System call to control a message queue
uint64
sys_msgctl(void)
{
  int msqid, cmd;
  uint64 buf;

  if(argint(0, &msqid) < 0 || argint(1, &cmd) < 0 || argaddr(2, &buf) < 0)
    return -1;

  // 对于IPC_RMID命令，buf可以为0
  if(cmd != 1) { // IPC_RMID
    return -1; // 其他命令暂不支持
  }

  return msgctl(msqid, cmd, (void *)buf);
}