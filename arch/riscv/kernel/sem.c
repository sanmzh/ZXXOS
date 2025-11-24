
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "syscall.h"

// 信号量数组
struct sem_set sem_sets[MAX_SEM_SETS];
struct spinlock sem_lock;

// 定义锁的获取顺序以避免死锁
// 顺序：进程锁(p->lock) -> 全局锁(sem_lock) -> 信号量集锁(set.lock)

// 初始化信号量子系统
void
sem_init(void)
{
  initlock(&sem_lock, "sem");

  // 初始化所有信号量集
  for(int i = 0; i < MAX_SEM_SETS; i++) {
    sem_sets[i].used = 0;
    sem_sets[i].refcnt = 0;
    sem_sets[i].semid = 0;
    sem_sets[i].marked_for_deletion = 0;
    initlock(&sem_sets[i].lock, "sem_set");
  }
}

// 通过semid查找信号量集
// 注意：调用者必须先获取全局锁(sem_lock)
static struct sem_set*
sem_find_by_id_locked(int semid)
{
  struct sem_set *set = 0;

  for(int i = 0; i < MAX_SEM_SETS; i++) {
    if(sem_sets[i].used && sem_sets[i].semid == semid) {
      set = &sem_sets[i];
      break;
    }
  }

  return set;
}

// 生成唯一的semid
// 注意：调用者必须先获取全局锁(sem_lock)
static int
sem_generate_id_locked(void)
{
  static int next_id = 1;
  int id;

  id = next_id++;
  // 避免ID溢出，如果溢出则从1重新开始
  if(next_id <= 0) {
    next_id = 1;
  }

  return id;
}

// semget: 创建或获取信号量集标识符
int
semget(int key, int nsems, int semflg)
{
  // 检查信号量数量是否有效
  if(nsems <= 0 || nsems > MAX_SEMS_PER_SET)
    return -1;

  // 按照锁顺序，先获取全局锁
  acquire(&sem_lock);

  // 查找或创建信号量集
  int create = (semflg & 0x01000) ? 1 : 0;  // IPC_CREAT
  struct sem_set *set = 0;
  struct sem_set *unused = 0;

  // 查找已存在的信号量集或空闲位置
  for(int i = 0; i < MAX_SEM_SETS; i++) {
    if(sem_sets[i].used && !sem_sets[i].marked_for_deletion) {
      if(sem_sets[i].key == key) {
        // 找到已存在的信号量集
        set = &sem_sets[i];
        // 检查信号量数量是否匹配
        if(set->nsems != nsems) {
          release(&sem_lock);
          return -1;
        }
        break;
      }
    } else if(!unused && !sem_sets[i].used) {
      // 记录第一个未使用的位置
      unused = &sem_sets[i];
    }
  }

  // 如果没有找到且需要创建
  if(!set && create && unused) {
    set = unused;
    set->used = 1;
    set->key = key;
    set->nsems = nsems;
    set->refcnt = 0;
    set->marked_for_deletion = 0;

    // 初始化所有信号量
    for(int i = 0; i < nsems; i++) {
      set->sems[i].value = 0;  // 默认值为0
      set->sems[i].pid = 0;    // 最后操作的进程ID
    }
  }

  if(!set) {
    // 创建失败或不存在
    release(&sem_lock);
    return -1;
  }

  // 按照锁顺序，再获取信号量集锁
  acquire(&set->lock);

  // 如果还没有分配semid，分配一个
  if(set->semid == 0) {
    set->semid = sem_generate_id_locked();
  }
  int semid = set->semid;

  // 释放锁
  release(&set->lock);
  release(&sem_lock);

  return semid;
}

// semop: 信号量操作
int
semop(int semid, struct sembuf *sops, unsigned nsops)
{
  struct proc *p = myproc();
  struct sem_set *set = 0;

  if(nsops <= 0 || nsops > MAX_SEM_OPS)
    return -1;

  // 按照锁顺序，先获取全局锁
  acquire(&sem_lock);

  // 查找信号量集
  set = sem_find_by_id_locked(semid);
  if(!set || set->marked_for_deletion) {
    release(&sem_lock);
    return -1;
  }

  // 按照锁顺序，再获取信号量集锁
  acquire(&set->lock);

  // 增加引用计数
  set->refcnt++;

  // 释放全局锁
  release(&sem_lock);

  // 复制用户空间的sembuf数组
  struct sembuf ops[MAX_SEM_OPS];
  if(copyin(p->pagetable, (char*)ops, (uint64)sops, nsops * sizeof(struct sembuf)) < 0) {
    set->refcnt--;
    release(&set->lock);
    return -1;
  }

  // 执行所有操作
  for(int i = 0; i < nsops; i++) {
    int sem_num = ops[i].sem_num;
    short sem_op = ops[i].sem_op;
    short sem_flg = ops[i].sem_flg;

    // 检查信号量编号是否有效
    if(sem_num < 0 || sem_num >= set->nsems) {
      set->refcnt--;
      release(&set->lock);
      return -1;
    }

    // 执行操作
    if(sem_op > 0) {
      // 增加信号量值
      set->sems[sem_num].value += sem_op;
      set->sems[sem_num].pid = p->pid;
      // 唤醒等待的进程
      wakeup(&set->sems[sem_num]);
    } else if(sem_op < 0) {
      // 减少信号量值
      int abs_op = -sem_op;

      // 检查是否可以立即执行操作
      if(set->sems[sem_num].value >= abs_op) {
        set->sems[sem_num].value -= abs_op;
        set->sems[sem_num].pid = p->pid;
      } else {
        // 不能立即执行，需要等待
        if(sem_flg & IPC_NOWAIT) {
          // 非阻塞模式，返回错误
          set->refcnt--;
          release(&set->lock);
          return -1;
        } else {
          // 阻塞模式，等待信号量可用
          while(set->sems[sem_num].value < abs_op) {
            // 释放信号量集锁，允许其他进程操作
            release(&set->lock);

            // 睡眠等待
            sleep(&set->sems[sem_num], &p->lock);

            // 重新获取信号量集锁
            acquire(&set->lock);

            // 检查信号量集是否被删除
            if(set->marked_for_deletion) {
              set->refcnt--;
              release(&set->lock);
              return -1;
            }
          }

          // 再次检查并执行操作
          if(set->sems[sem_num].value >= abs_op) {
            set->sems[sem_num].value -= abs_op;
            set->sems[sem_num].pid = p->pid;
          } else {
            // 不应该发生，但为了安全处理
            set->refcnt--;
            release(&set->lock);
            return -1;
          }
        }
      }
    } else {
      // sem_op == 0，等待信号量值为0
      if(set->sems[sem_num].value == 0) {
        // 已经是0，直接返回
        set->sems[sem_num].pid = p->pid;
      } else {
        // 需要等待
        if(sem_flg & IPC_NOWAIT) {
          // 非阻塞模式，返回错误
          set->refcnt--;
          release(&set->lock);
          return -1;
        } else {
          // 阻塞模式，等待信号量值为0
          while(set->sems[sem_num].value != 0) {
            // 释放信号量集锁，允许其他进程操作
            release(&set->lock);

            // 睡眠等待
            sleep(&set->sems[sem_num], &p->lock);

            // 重新获取信号量集锁
            acquire(&set->lock);

            // 检查信号量集是否被删除
            if(set->marked_for_deletion) {
              set->refcnt--;
              release(&set->lock);
              return -1;
            }
          }

          // 再次检查
          if(set->sems[sem_num].value == 0) {
            set->sems[sem_num].pid = p->pid;
          } else {
            // 不应该发生，但为了安全处理
            set->refcnt--;
            release(&set->lock);
            return -1;
          }
        }
      }
    }
  }

  // 减少引用计数
  set->refcnt--;

  release(&set->lock);

  return 0;
}

// semctl: 信号量控制操作
int
semctl(int semid, int semnum, int cmd, void *arg)
{
  // 按照锁顺序，先获取全局锁
  acquire(&sem_lock);

  // 查找信号量集
  struct sem_set *set = sem_find_by_id_locked(semid);
  if(!set) {
    release(&sem_lock);
    return -1;
  }

  // 检查信号量编号是否有效（对于特定信号量的操作）
  if((cmd == GETVAL || cmd == SETVAL || cmd == GETPID || cmd == GETNCNT || cmd == GETZCNT) && 
     (semnum < 0 || semnum >= set->nsems)) {
    release(&sem_lock);
    return -1;
  }

  switch(cmd) {
    case IPC_RMID: {  // 删除信号量集
      // 按照锁顺序，再获取信号量集锁
      acquire(&set->lock);

      // 标记为删除，但实际删除推迟到引用计数为0时
      set->marked_for_deletion = 1;

      // 如果引用计数已经为0，立即删除
      if(set->refcnt == 0) {
        set->used = 0;
        set->marked_for_deletion = 0;
      }

      release(&set->lock);
      release(&sem_lock);
      return 0;
    }

    case GETVAL: {  // 获取信号量值
      // 按照锁顺序，再获取信号量集锁
      acquire(&set->lock);
      int val = set->sems[semnum].value;
      release(&set->lock);
      release(&sem_lock);
      return val;
    }

    case SETVAL: {  // 设置信号量值
      // 按照锁顺序，再获取信号量集锁
      acquire(&set->lock);
      int value;
      if(copyin(myproc()->pagetable, (char*)&value, (uint64)arg, sizeof(int)) < 0) {
        release(&set->lock);
        release(&sem_lock);
        return -1;
      }
      set->sems[semnum].value = value;
      set->sems[semnum].pid = myproc()->pid;
      // 唤醒等待的进程
      wakeup(&set->sems[semnum]);
      release(&set->lock);
      release(&sem_lock);
      return 0;
    }

    case GETPID: {  // 获取最后操作信号量的进程ID
      // 按照锁顺序，再获取信号量集锁
      acquire(&set->lock);
      int pid = set->sems[semnum].pid;
      release(&set->lock);
      release(&sem_lock);
      return pid;
    }

    case GETNCNT: {  // 获取等待信号量值增加的进程数
      // 简化实现，返回0
      release(&sem_lock);
      return 0;
    }

    case GETZCNT: {  // 获取等待信号量值变为0的进程数
      // 简化实现，返回0
      release(&sem_lock);
      return 0;
    }

    case GETALL: {  // 获取所有信号量的值
      // 按照锁顺序，再获取信号量集锁
      acquire(&set->lock);

      // 复制到用户空间
      if(copyout(myproc()->pagetable, (uint64)arg, (char*)set->sems, 
                 set->nsems * sizeof(struct sem)) < 0) {
        release(&set->lock);
        release(&sem_lock);
        return -1;
      }

      release(&set->lock);
      release(&sem_lock);
      return 0;
    }

    case SETALL: {  // 设置所有信号量的值
      // 按照锁顺序，再获取信号量集锁
      acquire(&set->lock);

      // 从用户空间复制
      struct sem temp_sems[MAX_SEMS_PER_SET];
      if(copyin(myproc()->pagetable, (char*)temp_sems, (uint64)arg, 
                set->nsems * sizeof(struct sem)) < 0) {
        release(&set->lock);
        release(&sem_lock);
        return -1;
      }

      // 设置值并唤醒等待的进程
      for(int i = 0; i < set->nsems; i++) {
        set->sems[i].value = temp_sems[i].value;
        set->sems[i].pid = myproc()->pid;
        wakeup(&set->sems[i]);
      }

      release(&set->lock);
      release(&sem_lock);
      return 0;
    }

    case IPC_STAT: {  // 获取信号量集信息
      struct semid_ds *buf = (struct semid_ds*)arg;

      if(!buf) {
        release(&sem_lock);
        return -1;
      }

      // 按照锁顺序，再获取信号量集锁
      acquire(&set->lock);

      // 填充信息
      buf->sem_perm.key = set->key;
      buf->sem_perm.seq = set->semid;
      buf->sem_nsems = set->nsems;
      buf->sem_otime = 0;  // 简化实现
      buf->sem_ctime = 0;  // 简化实现

      release(&set->lock);
      release(&sem_lock);

      return 0;
    }

    default:
      release(&sem_lock);
      return -1;
  }
}
