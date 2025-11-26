#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MSGSIZE 32
#define STRESS_MSGSIZE 64
#define NUM_MSGS 50
#define NUM_PROCS 3

struct msgbuf {
  long mtype;
  char mtext[STRESS_MSGSIZE];  // 修复：使用STRESS_MSGSIZE确保足够大
};

void
test_basic_msg_queue()
{
  int msgid;
  struct msgbuf msg;

  printf("=== 基本消息队列测试 ===\n");
  
  // 创建消息队列
  msgid = msgget(0x2000, 0x01000); // IPC_CREAT
  if(msgid < 0) {
    printf("msgget 失败\n");
    exit(1);
  }
  printf("msgid = %d\n", msgid);

  // 发送消息
  msg.mtype = 1;
  strcpy(msg.mtext, "hello world");
  if(msgsnd(msgid, &msg, strlen(msg.mtext), 0) < 0) {
    printf("msgsnd 失败\n");
    exit(1);
  }
  printf("发送消息: %s\n", msg.mtext);

  // 接收消息
  memset(&msg, 0, sizeof(msg));
  int len = msgrcv(msgid, &msg, MSGSIZE, 1, 0);
  if(len < 0) {
    printf("msgrcv 失败\n");
    exit(1);
  }
  printf("接收消息: %s (长度=%d)\n", msg.mtext, len);

  // 删除消息队列
  if(msgctl(msgid, 1, 0) < 0) { // IPC_RMID
    printf("msgctl 失败\n");
    exit(1);
  }
  printf("消息队列已删除\n");
}

void
test_multi_message()
{
  int msgid;
  struct msgbuf msg;

  printf("\n=== 多消息测试 ===\n");
  
  // 创建消息队列
  msgid = msgget(0x2001, 0x01000); // IPC_CREAT
  if(msgid < 0) {
    printf("msgget 失败\n");
    exit(1);
  }
  printf("msgid = %d\n", msgid);

  // 发送多个消息
  for(int i = 0; i < 5; i++) {
    msg.mtype = i + 1;
    strcpy(msg.mtext, "message ");
    int len = strlen(msg.mtext);
    msg.mtext[len] = '0' + i;
    msg.mtext[len + 1] = '\0';
    if(msgsnd(msgid, &msg, strlen(msg.mtext), 0) < 0) {
      printf("msgsnd 失败\n");
      exit(1);
    }
    printf("发送消息: %s\n", msg.mtext);
  }

  // 接收多个消息
  for(int i = 1; i <= 5; i++) {
    memset(&msg, 0, sizeof(msg));
    int len = msgrcv(msgid, &msg, MSGSIZE, i, 0);
    if(len < 0) {
      printf("msgrcv 失败\n");
      exit(1);
    }
    printf("接收消息: %s (类型=%d)\n", msg.mtext, (int)msg.mtype);
  }

  // 删除消息队列
  if(msgctl(msgid, 1, 0) < 0) { // IPC_RMID
    printf("msgctl 失败\n");
    exit(1);
  }
  printf("消息队列已删除\n");
}

void
test_fork()
{
  int msgid;
  struct msgbuf msg;

  printf("\n=== 进程间通信测试 ===\n");
  
  // 创建消息队列
  msgid = msgget(0x2002, 0x01000); // IPC_CREAT
  if(msgid < 0) {
    printf("msgget 失败\n");
    exit(1);
  }
  printf("msgid = %d\n", msgid);

  int pid = fork();
  if(pid < 0) {
    printf("fork 失败\n");
    exit(1);
  }

  if(pid == 0) {
    // 子进程 - 发送消息
    msg.mtype = 1;
    strcpy(msg.mtext, "hello from child");
    if(msgsnd(msgid, &msg, strlen(msg.mtext), 0) < 0) {
      printf("child: msgsnd 失败\n");
      exit(1);
    }
    exit(0);
  } else {
    // 父进程 - 接收消息
    memset(&msg, 0, sizeof(msg));
    int len = msgrcv(msgid, &msg, MSGSIZE, 1, 0);
    if(len < 0) {
      printf("parent: msgrcv 失败\n");
      exit(1);
    }
    
    // 等待子进程结束后再输出，避免竞争
    wait(0);
    
    printf("parent: 接收消息: %s (长度=%d)\n", msg.mtext, len);
    
    // 删除消息队列
    if(msgctl(msgid, 1, 0) < 0) { // IPC_RMID
      printf("parent: msgctl 失败\n");
      exit(1);
    }
    printf("parent: 消息队列已删除\n");
  }
}

void
stress_receiver(int msgid, int proc_id)
{
  struct msgbuf msg;
  int count = 0;
  
  printf("接收进程 %d 启动\n", proc_id);
  
  for (int i = 0; i < NUM_MSGS; i++) {
    memset(&msg, 0, sizeof(msg));
    // 使用STRESS_MSGSIZE作为接收缓冲区大小
    printf("接收进程 %d: 尝试接收第 %d 条消息，期望类型 %d\n", proc_id, i+1, proc_id);
    int len = msgrcv(msgid, &msg, STRESS_MSGSIZE, proc_id, 0);
    if (len < 0) {
      printf("接收进程 %d: msgrcv 失败\n", proc_id);
      exit(1);
    }
    
    // 验证接收到的消息类型是否正确
    if (msg.mtype != proc_id) {
      printf("接收进程 %d: 接收到错误类型的消息，期望 %d，实际 %d\n", proc_id, proc_id, (int)msg.mtype);
      exit(1);
    }
    
    count++;
    
    // 每接收10条消息打印一次进度
    if (count % 10 == 0) {
      printf("接收进程 %d: 已接收 %d 条消息\n", proc_id, count);
    }
  }
  
  printf("接收进程 %d 完成，共接收 %d 条消息\n", proc_id, count);
  exit(0);
}

void
stress_sender(int msgid, int proc_id)
{
  struct msgbuf msg;
  
  printf("发送进程 %d 启动\n", proc_id);
  
  for (int i = 0; i < NUM_MSGS; i++) {
    // 修复：确保消息类型与接收进程ID匹配
    msg.mtype = proc_id;  // 每个发送进程发送给对应ID的接收进程
  
    // 构造消息内容
    memset(msg.mtext, 'A' + proc_id, STRESS_MSGSIZE - 1);
    msg.mtext[STRESS_MSGSIZE - 1] = '\0';
    
    // 使用STRESS_MSGSIZE-1作为消息长度，确保不包括结尾的'\0'
    printf("发送进程 %d: 尝试发送第 %d 条消息，类型 %d\n", proc_id, i+1, (int)msg.mtype);
    if (msgsnd(msgid, &msg, STRESS_MSGSIZE - 1, 0) < 0) {
      printf("发送进程 %d: msgsnd 失败\n", proc_id);
      exit(1);
    }
    
    // 每发送10条消息打印一次进度
    if ((i + 1) % 10 == 0) {
      printf("发送进程 %d: 已发送 %d 条消息\n", proc_id, i + 1);
    }
  }
  
  printf("发送进程 %d 完成\n", proc_id);
  exit(0);
}

void
test_stress()
{
  int msgid;
  
  printf("\n=== 消息队列压力测试 ===\n");
  printf("创建 %d 个发送进程和 %d 个接收进程，每个进程处理 %d 条消息\n", 
         NUM_PROCS, NUM_PROCS, NUM_MSGS);
  
  // 创建消息队列
  msgid = msgget(0x3000, 0x01000); // IPC_CREAT
  if (msgid < 0) {
    printf("msgget 失败\n");
    exit(1);
  }
  printf("msgid = %d\n", msgid);
  
  // 创建发送进程
  for (int i = 0; i < NUM_PROCS; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("fork 发送进程失败\n");
      exit(1);
    }
    if (pid == 0) {
      stress_sender(msgid, i + 1);
    }
  }
  
  // 创建接收进程
  for (int i = 0; i < NUM_PROCS; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("fork 接收进程失败\n");
      exit(1);
    }
    if (pid == 0) {
      stress_receiver(msgid, i + 1);
    }
  }
  
  // 等待所有子进程结束
  for (int i = 0; i < NUM_PROCS * 2; i++) {
    wait(0);
  }
  
  // 确保所有子进程都完成后再删除消息队列
  sleep(1);
  
  // 删除消息队列
  if (msgctl(msgid, 1, 0) < 0) { // IPC_RMID
    printf("msgctl 删除消息队列失败\n");
    exit(1);
  }
  
  printf("压力测试完成，消息队列已删除\n");
}

void
advanced_stress_test()
{
  int msgid;
  const int NUM_ADVANCED_PROCS = 4;
  const int NUM_ADVANCED_MSGS = 100;
  
  printf("\n=== 高级消息队列压力测试 ===\n");
  printf("创建 %d 个发送进程和 %d 个接收进程，每个进程处理 %d 条消息\n",
         NUM_ADVANCED_PROCS, NUM_ADVANCED_PROCS, NUM_ADVANCED_MSGS);
  
  // 创建消息队列
  msgid = msgget(0x4000, 0x01000); // IPC_CREAT
  if (msgid < 0) {
    printf("msgget 失败\n");
    exit(1);
  }
  printf("msgid = %d\n", msgid);
  
  // 创建发送进程（每个发送进程只发送给对应的接收进程）
  for (int i = 0; i < NUM_ADVANCED_PROCS; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("fork 发送进程失败\n");
      exit(1);
    }
    if (pid == 0) {
      struct msgbuf msg;
      int send_type = i + 1;  // 每个发送进程发送给对应的接收进程
      printf("发送进程 %d 启动，发送消息类型 %d\n", i + 1, send_type);
      
      // 发送消息
      for (int j = 0; j < NUM_ADVANCED_MSGS; j++) {
        msg.mtype = send_type;  // 发送给对应的接收进程
        
        // 构造消息内容
        memset(msg.mtext, 'A' + i, 32);
        msg.mtext[32] = '\0';
        
        printf("发送进程 %d: 准备发送第 %d 条消息，类型 %d\n", i + 1, j + 1, send_type);
        if (msgsnd(msgid, &msg, 32, 0) < 0) {
          printf("发送进程 %d: msgsnd 失败\n", i + 1);
          exit(1);
        }
        printf("发送进程 %d: 成功发送第 %d 条消息\n", i + 1, j + 1);
        
        // 每发送10条消息打印一次进度
        if ((j + 1) % 10 == 0) {
          printf("发送进程 %d: 已发送 %d 条消息\n", i + 1, j + 1);
        }
      }
      printf("发送进程 %d 完成\n", i + 1);
      exit(0);
    }
  }
  
  // 创建接收进程（每个接收进程只接收特定类型的消息）
  for (int i = 0; i < NUM_ADVANCED_PROCS; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("fork 接收进程失败\n");
      exit(1);
    }
    if (pid == 0) {
      struct msgbuf msg;
      int expected_type = i + 1;
      int count = 0;
      
      printf("接收进程 %d 启动，期望接收类型 %d\n", i + 1, expected_type);
      
      // 接收消息直到达到总数
      for (int j = 0; j < NUM_ADVANCED_MSGS; j++) {
        memset(&msg, 0, sizeof(msg));
        printf("接收进程 %d: 准备接收第 %d 条消息，期望类型 %d\n", i + 1, j + 1, expected_type);
        int len = msgrcv(msgid, &msg, STRESS_MSGSIZE, expected_type, 0);
        if (len < 0) {
          printf("接收进程 %d: msgrcv 失败\n", i + 1);
          exit(1);
        }
        
        // 验证消息类型
        if (msg.mtype != expected_type) {
          printf("接收进程 %d: 接收到错误类型的消息，期望 %d，实际 %d\n", 
                 i + 1, expected_type, (int)msg.mtype);
          exit(1);
        }
        
        count++;
        printf("接收进程 %d: 成功接收第 %d 条消息\n", i + 1, count);
        
        // 每接收10条消息打印一次进度
        if (count % 10 == 0) {
          printf("接收进程 %d: 已接收 %d 条消息\n", i + 1, count);
        }
      }
      
      printf("接收进程 %d 完成，共接收 %d 条消息\n", i + 1, count);
      exit(0);
    }
  }
  
  // 等待所有子进程结束
  for (int i = 0; i < NUM_ADVANCED_PROCS * 2; i++) {
    wait(0);
  }
  
  // 确保所有子进程都完成后再删除消息队列
  sleep(1);
  
  // 删除消息队列
  if (msgctl(msgid, 1, 0) < 0) { // IPC_RMID
    printf("msgctl 删除消息队列失败\n");
    exit(1);
  }
  
  printf("高级压力测试完成，消息队列已删除\n");
}

int
main(void)
{
  printf("消息队列测试程序\n");

  test_basic_msg_queue();
  test_multi_message();
  test_fork();
  test_stress();
  advanced_stress_test();

  printf("\n所有测试通过！\n");
  exit(0);
}