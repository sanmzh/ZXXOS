
#ifndef _SEM_H
#define _SEM_H

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

// IPC命令
#define IPC_CREAT  0x01000
#define IPC_NOWAIT 0x08000
#define IPC_RMID   1

// 信号量控制命令
#define GETVAL  12
#define SETVAL  13
#define GETPID  14
#define GETNCNT 15
#define GETZCNT 16
#define GETALL  17
#define SETALL  18
#define IPC_STAT 19

#endif
