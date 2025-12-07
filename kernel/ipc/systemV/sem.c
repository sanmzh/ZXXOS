
#include "types.h"
#include "param.h"
#include "memlayout.h"
#ifdef riscv
#include "riscv.h"
#endif
#ifdef loongarch
#include "loongarch.h"
#endif
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "syscall.h"
#include "ipc.h"

// 信号量数组
struct sem_set sem_sets[MAX_SEM_SETS];
struct spinlock sem_lock;

// 定义锁的获取顺序以避免死锁
// 顺序：进程锁(p->lock) -> 全局锁(sem_lock) -> 信号量集锁(set.lock)

// 初始化信号量子系统
void
sem_init(void)
{
  // printf("sem_init: initializing semaphore subsystem\n");
  initlock(&sem_lock, "sem");

  // 初始化所有信号量集
  for(int i = 0; i < MAX_SEM_SETS; i++) {
    sem_sets[i].used = 0;
    sem_sets[i].refcnt = 0;
    sem_sets[i].semid = 0;
    sem_sets[i].marked_for_deletion = 0;
    initlock(&sem_sets[i].lock, "sem_set");
  }
  // printf("sem_init: initialized %d semaphore sets\n", MAX_SEM_SETS);
}

// 通过semid查找信号量集
// 注意：调用者必须先获取全局锁(sem_lock)
static struct sem_set*
sem_find_by_id_locked(int semid)
{
  // printf("sem_find_by_id_locked: searching for semid %d\n", semid);
  struct sem_set *set = 0;

  for(int i = 0; i < MAX_SEM_SETS; i++) {
    if(sem_sets[i].used && sem_sets[i].semid == semid) {
      set = &sem_sets[i];
      //printf("sem_find_by_id_locked: found set at index %d\n", i);
      break;
    }
  }

  //if (!set) {
    //printf("sem_find_by_id_locked: set not found\n");
  //}
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
  //printf("semget: key=%d, nsems=%d, semflg=0x%x\n", key, nsems, semflg);
  
  // 检查信号量数量是否有效
  if(nsems <= 0 || nsems > MAX_SEMS_PER_SET) {
    //printf("semget: invalid nsems %d\n", nsems);
    return -1;
  }

  // 按照锁顺序，先获取全局锁
  acquire(&sem_lock);
  //printf("semget: acquired global lock\n");

  // 查找或创建信号量集
  int create = (semflg & 0x01000) ? 1 : 0;  // IPC_CREAT
  struct sem_set *set = 0;
  struct sem_set *unused = 0;

  // 查找已存在的信号量集或空闲位置
  for(int i = 0; i < MAX_SEM_SETS; i++) {
    if(sem_sets[i].used && sem_sets[i].key == key && !sem_sets[i].marked_for_deletion) {
      // 找到已存在的信号量集
      set = &sem_sets[i];
      //printf("semget: found existing semaphore set with id %d\n", set->semid);
      // 检查信号量数量是否匹配
      if(set->nsems != nsems) {
        //printf("semget: nsems mismatch: expected %d, found %d\n", nsems, set->nsems);
        release(&sem_lock);
        return -1;
      }
      break;
    } else if(!unused && !sem_sets[i].used) {
      // 记录第一个未使用的位置
      unused = &sem_sets[i];
    }
  }

  // 如果没有找到且需要创建
  if(!set && create && unused) {
    //printf("semget: creating new semaphore set\n");
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
    //printf("semget: failed to find or create semaphore set\n");
    release(&sem_lock);
    return -1;
  }

  // 按照锁顺序，再获取信号量集锁
  acquire(&set->lock);
  //printf("semget: acquired set lock\n");

  // 如果还没有分配semid，分配一个
  if(set->semid == 0) {
    set->semid = sem_generate_id_locked();
    //printf("semget: assigned new semid %d\n", set->semid);
  }
  int semid = set->semid;

  // 释放锁
  release(&set->lock);
  release(&sem_lock);
  //printf("semget: released locks, returning semid %d\n", semid);

  return semid;
}

// semop: 信号量操作
int
semop(int semid, struct sembuf *sops, unsigned nsops)
{
  // printf("semop: semid=%d, sops=0x%p, nsops=%d\n", semid, sops, nsops);
  
  struct proc *p = myproc();
  struct sem_set *set = 0;

  if(nsops <= 0 || nsops > MAX_SEM_OPS) {
    // printf("semop: invalid nsops %d\n", nsops);
    return -1;
  }

  // 按照锁顺序，先获取全局锁
  acquire(&sem_lock);
  // printf("semop: acquired global lock\n");

  // 查找信号量集
  set = sem_find_by_id_locked(semid);
  if(!set || set->marked_for_deletion) {
    // printf("semop: semaphore set not found or marked for deletion\n");
    release(&sem_lock);
    return -1;
  }
  // printf("semop: found semaphore set with %d semaphores\n", set->nsems);

  // 按照锁顺序，再获取信号量集锁
  acquire(&set->lock);
  // printf("semop: acquired set lock\n");

  // 增加引用计数
  set->refcnt++;
  // printf("semop: refcnt incremented to %d\n", set->refcnt);

  // 释放全局锁
  release(&sem_lock);
  // printf("semop: released global lock\n");

  // 先释放信号量集锁以进行用户空间内存访问，避免死锁
  release(&set->lock);
  // printf("semop: released set lock for user memory access\n");

  // 复制用户空间的sembuf数组
  struct sembuf ops[MAX_SEM_OPS];
  // printf("semop: copying sembuf array from user space\n");
  if(copyin(p->pagetable, (char*)ops, (uint64)sops, nsops * sizeof(struct sembuf)) < 0) {
    // printf("semop: copyin failed\n");
    set->refcnt--;
    acquire(&set->lock);  // 重新获取锁以安全地减少引用计数
    set->refcnt--;
    release(&set->lock);
    return -1;
  }
  // printf("semop: copied sembuf array from user space\n");

  // 重新获取信号量集锁以继续操作
  acquire(&set->lock);
  // printf("semop: re-acquired set lock after user memory access\n");

  // 执行所有操作
  for(int i = 0; i < nsops; i++) {
    int sem_num = ops[i].sem_num;
    short sem_op = ops[i].sem_op;
    short sem_flg = ops[i].sem_flg;
    
    // printf("semop: operation %d: sem_num=%d, sem_op=%d, sem_flg=0x%x\n", 
    //       i, sem_num, sem_op, sem_flg);

    // 检查信号量编号是否有效
    if(sem_num < 0 || sem_num >= set->nsems) {
      // printf("semop: invalid semaphore number %d\n", sem_num);
      set->refcnt--;
      release(&set->lock);
      return -1;
    }

    // 执行操作
    if(sem_op > 0) {
      // 增加信号量值
      set->sems[sem_num].value += sem_op;
      set->sems[sem_num].pid = p->pid;
      // printf("semop: increased sem[%d] by %d to %d\n", 
      //       sem_num, sem_op, set->sems[sem_num].value);
      // 只有在之前值<=0且现在值>0时才唤醒等待的进程
      if (set->sems[sem_num].value - sem_op <= 0 && set->sems[sem_num].value > 0) {
        // 唤醒等待的进程，使用信号量集锁
        // printf("Process %d increasing semaphore value to %d, calling wakeup\n", p->pid, set->sems[sem_num].value);
        wakeup(&set->sems[sem_num]);
      }
    } else if(sem_op < 0) {
      // 减少信号量值
      int abs_op = -sem_op;
      // printf("semop: decreasing sem[%d] by %d\n", sem_num, abs_op);

      // 检查是否可以立即执行操作
      if(set->sems[sem_num].value >= abs_op) {
        set->sems[sem_num].value -= abs_op;
        set->sems[sem_num].pid = p->pid;
        // printf("semop: decreased sem[%d] to %d\n", sem_num, set->sems[sem_num].value);
      } else {
        // 不能立即执行，需要等待
        // printf("semop: not enough resources, need to wait\n");
        if(sem_flg & IPC_NOWAIT) {
          // 非阻塞模式，返回错误
          // printf("semop: IPC_NOWAIT flag set, returning error\n");
          set->refcnt--;
          release(&set->lock);
          return -1;
        } else {
          // 阻塞模式，等待信号量可用
          // 先保存当前值，避免竞争条件
          int current_value = set->sems[sem_num].value;
          // printf("Process %d waiting for semaphore, current value: %d, need: %d\n", p->pid, current_value, abs_op);
          
          while(current_value < abs_op) {
            // 睡眠等待，传递信号量集锁
            // printf("Process %d going to sleep on semaphore\n", p->pid);
            sleep(&set->sems[sem_num], &set->lock);
            // printf("Process %d woke up from sleep\n", p->pid);
            
            // 重新获取信号量集锁（sleep返回时已经获取）
            // 更新当前值
            current_value = set->sems[sem_num].value;
            // printf("Process %d woke up, new semaphore value: %d\n", p->pid, current_value);

            // 检查信号量集是否被删除
            if(set->marked_for_deletion) {
              // printf("semop: semaphore set marked for deletion\n");
              set->refcnt--;
              release(&set->lock);
              return -1;
            }
          }

          // 执行操作
          set->sems[sem_num].value -= abs_op;
          set->sems[sem_num].pid = p->pid;
          // printf("semop: finally decreased sem[%d] to %d\n", sem_num, set->sems[sem_num].value);
        }
      }
    } else {
      // sem_op == 0，等待信号量值为0
      // printf("semop: waiting for sem[%d] to become zero\n", sem_num);
      if(set->sems[sem_num].value == 0) {
        // 已经是0，直接返回
        // printf("semop: sem[%d] is already zero\n", sem_num);
        set->sems[sem_num].pid = p->pid;
      } else {
        // 需要等待
        if(sem_flg & IPC_NOWAIT) {
          // 非阻塞模式，返回错误
          // printf("semop: IPC_NOWAIT flag set, returning error\n");
          set->refcnt--;
          release(&set->lock);
          return -1;
        } else {
          // 阻塞模式，等待信号量值为0
          // printf("semop: waiting for sem[%d] to reach zero\n", sem_num);
          
          while(set->sems[sem_num].value != 0) {
            // 睡眠等待，传递信号量集锁
            // printf("semop: sleeping on sem[%d] waiting for zero\n", sem_num);
            sleep(&set->sems[sem_num], &set->lock);
            // printf("semop: woke up from waiting for sem[%d] to reach zero\n", sem_num);
            
            // 重新获取信号量集锁（sleep返回时已经获取）
            // printf("semop: sem[%d] current value: %d\n", sem_num, set->sems[sem_num].value);

            // 检查信号量集是否被删除
            if(set->marked_for_deletion) {
              // printf("semop: semaphore set marked for deletion\n");
              set->refcnt--;
              release(&set->lock);
              return -1;
            }
          }

          // 执行操作
          set->sems[sem_num].pid = p->pid;
          // printf("semop: sem[%d] reached zero\n", sem_num);
        }
      }
    }
  }

  // 减少引用计数
  set->refcnt--;
  // printf("semop: refcnt decremented to %d\n", set->refcnt);

  release(&set->lock);
  // printf("semop: released set lock, returning 0\n");

  return 0;
}

// semctl: 信号量控制操作
int
semctl(int semid, int semnum, int cmd, void *arg)
{
  //printf("semctl: semid=%d, semnum=%d, cmd=%d, arg=0x%p\n", semid, semnum, cmd, arg);

  // 检查信号量集标识符是否有效
  if(semid < 0) {
    //printf("semctl: invalid semid %d\n", semid);
    return -1;
  }

  // 按照锁顺序，先获取全局锁
  acquire(&sem_lock);
  //printf("semctl: acquired global lock\n");

  // 查找信号量集
  struct sem_set *set = sem_find_by_id_locked(semid);
  if(!set || set->marked_for_deletion) {
    //printf("semctl: semaphore set not found or marked for deletion\n");
    release(&sem_lock);
    return -1;
  }

  // 检查信号量编号是否有效（对于需要编号的命令）
  if(cmd != IPC_RMID && cmd != IPC_STAT && cmd != GETALL && cmd != SETALL) {
    if(semnum < 0 || semnum >= set->nsems) {
      //printf("semctl: invalid semnum %d\n", semnum);
      release(&sem_lock);
      return -1;
    }
  }

  switch(cmd) {
    case IPC_RMID: {  // 删除信号量集
      //printf("IPC_RMID: semid=%d\n", semid);
      
      // 按照锁顺序，再获取信号量集锁
      acquire(&set->lock);
      //printf("IPC_RMID: acquired set lock\n");

      // 标记为删除，但实际删除推迟到引用计数为0时
      set->marked_for_deletion = 1;
      //printf("IPC_RMID: marked for deletion, refcnt=%d\n", set->refcnt);

      // 如果引用计数已经为0，立即删除
      if(set->refcnt == 0) {
        //printf("IPC_RMID: refcnt is zero, deleting immediately\n");
        set->used = 0;
        set->marked_for_deletion = 0;
      }

      release(&set->lock);
      release(&sem_lock);
      //printf("IPC_RMID: released locks, returning 0\n");
      return 0;
    }

    case GETVAL: {  // 获取信号量值
      // 按照锁顺序，再获取信号量集锁
      acquire(&set->lock);
      int val = set->sems[semnum].value;
      release(&set->lock);
      release(&sem_lock);
      //printf("GETVAL: returning %d\n", val);
      return val;
    }

    case SETVAL: {  // 设置信号量值
      //printf("SETVAL: semid=%d, semnum=%d, arg=0x%p\n", semid, semnum, arg);
      
      // 按照锁顺序，再获取信号量集锁
      acquire(&set->lock);
      //printf("SETVAL: acquired set lock\n");
      
      int value;
      if(copyin(myproc()->pagetable, (char*)&value, (uint64)arg, sizeof(int)) < 0) {
        //printf("SETVAL: copyin failed\n");
        release(&set->lock);
        release(&sem_lock);
        return -1;
      }
      //printf("SETVAL: value from user space = %d\n", value);
      
      int old_value = set->sems[semnum].value;
      set->sems[semnum].value = value;
      set->sems[semnum].pid = myproc()->pid;
      //printf("SETVAL: updated semaphore %d from %d to %d\n", semnum, old_value, value);
      
      // 只有当信号量值增加时才唤醒等待的进程
      if (value > old_value) {
        // 唤醒等待信号量值增加的进程，使用信号量集锁
        //printf("Process %d setting semaphore value from %d to %d, calling wakeup\n", 
        //       myproc()->pid, old_value, set->sems[semnum].value);
        wakeup(&set->sems[semnum]);
      }
      
      release(&set->lock);
      release(&sem_lock);
      //printf("SETVAL: released locks, returning 0\n");
      return 0;
    }

    case GETPID: {  // 获取最后操作信号量的进程ID
      // 按照锁顺序，再获取信号量集锁
      acquire(&set->lock);
      int pid = set->sems[semnum].pid;
      release(&set->lock);
      release(&sem_lock);
      //printf("GETPID: returning %d\n", pid);
      return pid;
    }

    case GETNCNT: {  // 获取等待信号量值增加的进程数
      // 简化实现，返回0
      //printf("GETNCNT: returning 0\n");
      release(&sem_lock);
      return 0;
    }

    case GETZCNT: {  // 获取等待信号量值变为0的进程数
      // 简化实现，返回0
      //printf("GETZCNT: returning 0\n");
      release(&sem_lock);
      return 0;
    }

    case GETALL: {  // 获取所有信号量的值
      //printf("GETALL: semid=%d, arg=0x%p, nsems=%d\n", semid, arg, set->nsems);
      
      // 按照锁顺序，再获取信号量集锁
      acquire(&set->lock);
      //printf("GETALL: acquired set lock\n");

      // 创建临时数组只包含value值
      ushort values[MAX_SEMS_PER_SET];
      //printf("GETALL: preparing values array\n");
      for(int i = 0; i < set->nsems; i++) {
        values[i] = set->sems[i].value;
        //printf("GETALL: values[%d] = %d\n", i, values[i]);
      }

      // 复制到用户空间
      //printf("GETALL: copying to user space\n");
      if(copyout(myproc()->pagetable, (uint64)arg, (char*)values, 
                 set->nsems * sizeof(ushort)) < 0) {
        //printf("GETALL: copyout failed\n");
        release(&set->lock);
        release(&sem_lock);
        return -1;
      }
      //printf("GETALL: copied to user space\n");

      release(&set->lock);
      release(&sem_lock);
      //printf("GETALL: released locks, returning 0\n");
      return 0;
    }

    case SETALL: {  // 设置所有信号量的值
      //printf("SETALL: semid=%d, arg=0x%p, nsems=%d\n", semid, arg, set->nsems);
      
      // 按照锁顺序，再获取信号量集锁
      acquire(&set->lock);
      //printf("SETALL: acquired set lock\n");

      // 从用户空间复制
      struct sem temp_sems[MAX_SEMS_PER_SET];
      //printf("SETALL: copying from user space\n");
      if(copyin(myproc()->pagetable, (char*)temp_sems, (uint64)arg, 
                set->nsems * sizeof(struct sem)) < 0) {
        //printf("SETALL: copyin failed\n");
        release(&set->lock);
        release(&sem_lock);
        return -1;
      }
      //printf("SETALL: copied from user space\n");

      // 设置值并只在值增加时唤醒等待的进程
      for(int i = 0; i < set->nsems; i++) {
        int old_value = set->sems[i].value;
        set->sems[i].value = temp_sems[i].value;
        set->sems[i].pid = myproc()->pid;
        //printf("SETALL: sem[%d] updated from %d to %d\n", i, old_value, temp_sems[i].value);
        
        // 只有当信号量值增加时才唤醒等待的进程
        if (temp_sems[i].value > old_value) {
          //printf("SETALL: waking up processes waiting on sem[%d]\n", i);
          wakeup(&set->sems[i]);
        }
      }

      release(&set->lock);
      release(&sem_lock);
      //printf("SETALL: released locks, returning 0\n");
      return 0;
    }

    case IPC_STAT: {  // 获取信号量集信息
      //printf("IPC_STAT: semid=%d, arg=0x%p\n", semid, arg);
      struct semid_ds *buf = (struct semid_ds*)arg;

      if(!buf) {
        //printf("IPC_STAT: null buffer\n");
        release(&sem_lock);
        return -1;
      }

      // 按照锁顺序，再获取信号量集锁
      acquire(&set->lock);
      //printf("IPC_STAT: acquired set lock\n");

      // 填充信息到内核临时缓冲区
      struct semid_ds temp_buf;
      temp_buf.sem_perm.key = set->key;
      temp_buf.sem_perm.seq = set->semid;
      temp_buf.sem_nsems = set->nsems;
      temp_buf.sem_otime = 0;  // 简化实现
      temp_buf.sem_ctime = 0;  // 简化实现
      //printf("IPC_STAT: filled temp buffer - key=%d, seq=%d, nsems=%d\n", 
      //       temp_buf.sem_perm.key, temp_buf.sem_perm.seq, temp_buf.sem_nsems);

      // 释放锁
      release(&set->lock);
      release(&sem_lock);
      
      // 复制到用户空间
      //printf("IPC_STAT: copying to user space at 0x%p\n", buf);
      if(copyout(myproc()->pagetable, (uint64)buf, (char*)&temp_buf, sizeof(temp_buf)) < 0) {
        //printf("IPC_STAT: copyout failed\n");
        return -1;
      }
      //printf("IPC_STAT: copied to user space, returning 0\n");

      return 0;
    }

    default:
      //printf("semctl: unknown command %d\n", cmd);
      release(&sem_lock);
      return -1;
  }
}
