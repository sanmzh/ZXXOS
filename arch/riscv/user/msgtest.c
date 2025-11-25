#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MSGSIZE 32

struct msgbuf {
  long mtype;
  char mtext[MSGSIZE];
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
    printf("child: 发送消息: %s\n", msg.mtext);
    exit(0);
  } else {
    // 父进程 - 接收消息
    memset(&msg, 0, sizeof(msg));
    int len = msgrcv(msgid, &msg, MSGSIZE, 1, 0);
    if(len < 0) {
      printf("parent: msgrcv 失败\n");
      exit(1);
    }
    printf("parent: 接收消息: %s (长度=%d)\n", msg.mtext, len);
    
    // 等待子进程结束
    wait(0);
    
    // 删除消息队列
    if(msgctl(msgid, 1, 0) < 0) { // IPC_RMID
      printf("parent: msgctl 失败\n");
      exit(1);
    }
    printf("parent: 消息队列已删除\n");
  }
}

int
main(void)
{
  printf("消息队列测试程序\n");

  test_basic_msg_queue();
  test_multi_message();
  test_fork();

  printf("\n所有测试通过！\n");
  exit(0);
}