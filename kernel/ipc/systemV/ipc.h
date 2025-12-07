
#ifndef _IPC_H
#define _IPC_H

// #include "types.h"
// #include "spinlock.h"

// IPC命令
#define IPC_CREAT  0x01000
#define IPC_NOWAIT 0x08000
#define IPC_RMID   1
#define IPC_SET    2
#define IPC_STAT   3
#define IPC_PRIVATE 0

// 共享内存相关定义
#define SHM_NAME_LEN 32
#define MAX_SHM_REGIONS 16
#define MAX_SHM_ATTACH 16

// 共享内存区域结构体
struct shm_region {
  int used;                     // 是否已被使用
  int shmid;                    // 共享内存标识符
  int key;                      // 共享内存键值
  int size;                     // 共享内存大小（字节）
  int refcnt;                   // 引用计数
  uint64 pa;                    // 物理地址
  char name[SHM_NAME_LEN];      // 共享内存名称
  struct spinlock lock;         // 自旋锁
  int marked_for_deletion;      // 标记是否等待删除
};

// 共享内存附加区域
struct shm_attached {
  int used;          // 是否已被使用
  int shmid;         // 共享内存标识符
  uint64 va;         // 虚拟地址
};

// 信号量相关定义
#define MAX_SEMS_PER_SET 16
#define MAX_SEM_SETS 16
#define MAX_SEM_OPS 16

// 信号量结构体
struct sem {
  int value;    // 信号量值
  int pid;      // 最后操作信号量的进程ID
};

// 信号量集结构体
struct sem_set {
  int used;                     // 是否已被使用
  int semid;                    // 信号量集标识符
  int key;                      // 信号量集键值
  int nsems;                    // 信号量数量
  int refcnt;                   // 引用计数
  struct sem sems[MAX_SEMS_PER_SET];  // 信号量数组
  struct spinlock lock;         // 自旋锁
  int marked_for_deletion;      // 标记是否等待删除
};

// 信号量操作结构体
struct sembuf {
  short sem_num;  // 信号量编号
  short sem_op;   // 操作值
  short sem_flg;  // 操作标志
};

// 信号量集信息结构体
struct semid_ds {
  struct {
    int key;   // 键值
    int seq;   // 序列号
  } sem_perm;
  int sem_nsems;  // 信号量数量
  int sem_otime;  // 最后操作时间
  int sem_ctime;  // 创建时间
};

// 信号量控制命令
#define GETVAL  12
#define SETVAL  13
#define GETPID  14
#define GETNCNT 15
#define GETZCNT 16
#define GETALL  17
#define SETALL  18

// 信号量操作限制
#define MAX_SEM_OPS 16

// 消息队列相关定义
#define MAX_MSG_SIZE 512
#define MAX_MSG_QUEUE_SIZE 16
#define MAX_MSG_QUEUES 16

// 消息结构体
struct msg {
  struct msg *next;    // 下一条消息
  long type;           // 消息类型
  int size;            // 消息大小
  char data[MAX_MSG_SIZE]; // 消息数据
};

// 消息队列结构体
struct msg_queue {
  int used;                      // 是否已被使用
  int msqid;                     // 消息队列标识符
  int key;                       // 消息队列键值
  struct msg *head;              // 消息队列头指针
  struct msg *tail;              // 消息队列尾指针
  int msg_count;                 // 消息数量
  int max_bytes;                 // 队列最大字节数
  int refcnt;                    // 引用计数
  struct spinlock lock;          // 自旋锁
  int marked_for_deletion;       // 标记是否等待删除
};

// 消息缓冲区结构体
struct msgbuf {
  long mtype;         // 消息类型
  char mtext[1];      // 消息数据（变长）
};

// IPC系统函数声明
void shm_init(void);
int shmget(int key, int size, int shmflg);
void* shmat(int shmid, const void *addr, int shmflg);
int shmdt(const void *addr);
int shmctl(int shmid, int cmd, void *buf);

void sem_init(void);
int semget(int key, int nsems, int semflg);
int semop(int semid, struct sembuf *sops, unsigned nsops);
int semctl(int semid, int semnum, int cmd, void *arg);

void msg_init(void);
int msgget(int key, int msgflg);
int msgsnd(int msqid, const void *msgp, unsigned msgsz, int msgflg);
int msgrcv(int msqid, void *msgp, unsigned msgsz, int msgtyp, int msgflg);
int msgctl(int msqid, int cmd, void *buf);

#endif // _IPC_H
