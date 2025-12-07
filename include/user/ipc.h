
#ifndef _USER_IPC_H
#define _USER_IPC_H

// IPC命令
#define IPC_CREAT  0x01000
#define IPC_NOWAIT 0x08000
#define IPC_RMID   1
#define IPC_SET    2
#define IPC_STAT   3
#define IPC_PRIVATE 0

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

// 消息缓冲区结构体
struct msgbuf {
  long mtype;         // 消息类型
  char mtext[1];      // 消息数据（变长）
};

// 共享内存相关系统调用
int shmget(int key, int size, int shmflg);
void *shmat(int shmid, const void *addr, int shmflg);
int shmdt(const void *addr);
int shmctl(int shmid, int cmd, void *buf);

// 信号量相关系统调用
int semget(int key, int nsems, int semflg);
int semop(int semid, struct sembuf *sops, unsigned nsops);
int semctl(int semid, int semnum, int cmd, void *arg);

// 消息队列相关系统调用
int msgget(int key, int msgflg);
int msgsnd(int msqid, const void *msgp, unsigned msgsz, int msgflg);
int msgrcv(int msqid, void *msgp, unsigned msgsz, int msgtyp, int msgflg);
int msgctl(int msqid, int cmd, void *buf);

#endif // _USER_IPC_H
