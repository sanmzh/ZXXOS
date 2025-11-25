#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "syscall.h"

// 消息队列数组
struct msg_queue msg_queues[MAX_MSG_QUEUES];
struct spinlock msg_lock;

// 初始化消息队列子系统
void
msg_init(void)
{
  initlock(&msg_lock, "msg");

  // 初始化所有消息队列
  for(int i = 0; i < MAX_MSG_QUEUES; i++) {
    msg_queues[i].used = 0;
    msg_queues[i].refcnt = 0;
    msg_queues[i].msqid = 0;
    msg_queues[i].head = 0;
    msg_queues[i].tail = 0;
    msg_queues[i].msg_count = 0;
    msg_queues[i].max_bytes = 4096;
    msg_queues[i].marked_for_deletion = 0;
    initlock(&msg_queues[i].lock, "msg_queue");
  }
}

// 通过msqid查找消息队列
// 注意：调用者必须先获取全局锁(msg_lock)
static struct msg_queue*
msg_find_by_id_locked(int msqid)
{
  struct msg_queue *queue = 0;

  for(int i = 0; i < MAX_MSG_QUEUES; i++) {
    if(msg_queues[i].used && msg_queues[i].msqid == msqid) {
      queue = &msg_queues[i];
      break;
    }
  }

  return queue;
}

// 通过key查找消息队列
// 注意：调用者必须先获取全局锁(msg_lock)
static struct msg_queue*
msg_find_by_key_locked(int key)
{
  struct msg_queue *queue = 0;

  for(int i = 0; i < MAX_MSG_QUEUES; i++) {
    if(msg_queues[i].used && msg_queues[i].key == key) {
      queue = &msg_queues[i];
      break;
    }
  }

  return queue;
}

// 分配一个新的消息队列
// 注意：调用者必须先获取全局锁(msg_lock)
static struct msg_queue*
msg_alloc_locked(void)
{
  struct msg_queue *queue = 0;

  for(int i = 0; i < MAX_MSG_QUEUES; i++) {
    if(!msg_queues[i].used) {
      queue = &msg_queues[i];
      queue->used = 1;
      queue->refcnt = 1;
      queue->msqid = i + 1;  // msqid从1开始
      queue->head = 0;
      queue->tail = 0;
      queue->msg_count = 0;
      queue->marked_for_deletion = 0;
      break;
    }
  }

  return queue;
}

// 创建或获取消息队列
int
msgget(int key, int msgflg)
{
  struct msg_queue *queue;

  acquire(&msg_lock);

  // 查找是否存在具有相同key的消息队列
  if(key != IPC_PRIVATE) {
    queue = msg_find_by_key_locked(key);
  }

  // 如果找到了并且没有设置IPC_CREAT|IPC_EXCL标志，则返回该队列
  if(queue && !(msgflg & (IPC_CREAT | IPC_NOWAIT))) {
    queue->refcnt++;
    int msqid = queue->msqid;
    release(&msg_lock);
    return msqid;
  }

  // 如果没找到并且设置了IPC_CREAT标志，则创建新的消息队列
  if(!queue && (msgflg & IPC_CREAT)) {
    queue = msg_alloc_locked();
    if(queue) {
      queue->key = key;
    }
  }

  // 如果找到了但是设置了IPC_CREAT|IPC_EXCL标志，则返回错误
  if(queue && (msgflg & IPC_CREAT) && (msgflg & IPC_NOWAIT)) {
    release(&msg_lock);
    return -1;
  }

  int msqid = -1;
  if(queue) {
    msqid = queue->msqid;
  }

  release(&msg_lock);
  return msqid;
}

// 发送消息
int
msgsnd(int msqid, const void *msgp, unsigned msgsz, int msgflg)
{
  struct msg_queue *queue;
  struct msg *msg;
  struct msgbuf *mbuf = (struct msgbuf *)msgp;

  // 检查消息大小
  if(msgsz > MAX_MSG_SIZE) {
    return -1;
  }

  acquire(&msg_lock);

  // 查找消息队列
  queue = msg_find_by_id_locked(msqid);
  if(!queue) {
    release(&msg_lock);
    return -1;
  }

  // 增加引用计数
  queue->refcnt++;

  release(&msg_lock);

  // 获取队列锁
  acquire(&queue->lock);

  // 检查队列是否已满
  if(queue->msg_count >= MAX_MSG_QUEUE_SIZE) {
    if(msgflg & IPC_NOWAIT) {
      // 非阻塞模式，直接返回错误
      release(&queue->lock);
      
      acquire(&msg_lock);
      queue->refcnt--;
      release(&msg_lock);
      
      return -1;
    } else {
      // 阻塞模式，等待队列有空间
      while(queue->msg_count >= MAX_MSG_QUEUE_SIZE) {
        sleep(queue, &queue->lock);
      }
    }
  }

  // 分配新消息
  msg = (struct msg *)kalloc();
  if(!msg) {
    release(&queue->lock);
    
    acquire(&msg_lock);
    queue->refcnt--;
    release(&msg_lock);
    
    return -1;
  }

  // 初始化消息
  msg->next = 0;
  msg->type = mbuf->mtype;
  msg->size = msgsz;
  memmove(msg->data, mbuf->mtext, msgsz);

  // 添加到队列尾部
  if(queue->tail) {
    queue->tail->next = msg;
  } else {
    queue->head = msg;
  }
  queue->tail = msg;
  queue->msg_count++;

  // 唤醒等待接收消息的进程
  wakeup(queue);

  release(&queue->lock);

  // 减少引用计数
  acquire(&msg_lock);
  queue->refcnt--;
  release(&msg_lock);

  return 0;
}

// 接收消息
int
msgrcv(int msqid, void *msgp, unsigned msgsz, int msgtyp, int msgflg)
{
  struct msg_queue *queue;
  struct msg *msg, *prev;
  struct msgbuf *mbuf = (struct msgbuf *)msgp;

  acquire(&msg_lock);

  // 查找消息队列
  queue = msg_find_by_id_locked(msqid);
  if(!queue) {
    release(&msg_lock);
    return -1;
  }

  // 增加引用计数
  queue->refcnt++;

  release(&msg_lock);

  // 获取队列锁
  acquire(&queue->lock);

  // 查找符合条件的消息
  while(1) {
    msg = queue->head;
    prev = 0;

    // 遍历消息队列寻找合适的消息
    while(msg) {
      if(msgtyp == 0 || 
         (msgtyp > 0 && msg->type == msgtyp) ||
         (msgtyp < 0 && msg->type <= -msgtyp)) {
        break;
      }
      prev = msg;
      msg = msg->next;
    }

    // 如果找到了消息
    if(msg) {
      // 检查缓冲区大小
      if(msgsz < msg->size && !(msgflg & IPC_NOWAIT)) {
        release(&queue->lock);
        
        acquire(&msg_lock);
        queue->refcnt--;
        release(&msg_lock);
        
        return -1;
      }

      // 从队列中移除消息
      if(prev) {
        prev->next = msg->next;
      } else {
        queue->head = msg->next;
      }
      
      if(queue->tail == msg) {
        queue->tail = prev;
      }
      
      queue->msg_count--;

      // 唤醒等待发送消息的进程
      wakeup(queue);

      release(&queue->lock);

      // 复制消息到用户空间
      mbuf->mtype = msg->type;
      int copy_size = msg->size;
      if(msgsz < copy_size) {
        copy_size = msgsz;
      }
      memmove(mbuf->mtext, msg->data, copy_size);

      // 释放消息内存
      kfree((char *)msg);

      // 减少引用计数
      acquire(&msg_lock);
      queue->refcnt--;
      release(&msg_lock);

      return copy_size;
    }

    // 如果没有找到消息
    if(msgflg & IPC_NOWAIT) {
      // 非阻塞模式，直接返回错误
      release(&queue->lock);
      
      acquire(&msg_lock);
      queue->refcnt--;
      release(&msg_lock);
      
      return -1;
    } else {
      // 阻塞模式，等待有消息
      sleep(queue, &queue->lock);
    }
  }
}

// 控制消息队列
int
msgctl(int msqid, int cmd, void *buf)
{
  struct msg_queue *queue;

  acquire(&msg_lock);

  // 查找消息队列
  queue = msg_find_by_id_locked(msqid);
  if(!queue) {
    release(&msg_lock);
    return -1;
  }

  switch(cmd) {
    case IPC_RMID:
      // 标记为删除
      queue->marked_for_deletion = 1;
      
      // 如果没有其他进程在使用，则清理资源
      if(queue->refcnt <= 1) {
        // 删除队列中的所有消息
        struct msg *msg = queue->head;
        while(msg) {
          struct msg *next = msg->next;
          kfree((char *)msg);
          msg = next;
        }
        
        // 重置队列
        queue->used = 0;
        queue->refcnt = 0;
        queue->msqid = 0;
        queue->head = 0;
        queue->tail = 0;
        queue->msg_count = 0;
        queue->marked_for_deletion = 0;
      }
      break;
      
    case IPC_STAT:
      // TODO: 实现获取队列状态信息
      break;
      
    default:
      release(&msg_lock);
      return -1;
  }

  release(&msg_lock);
  return 0;
}