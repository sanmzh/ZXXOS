#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MSGSIZE 32
#define STRESS_MSGSIZE 64
#define NUM_MSGS 50
#define NUM_PROCS 10

struct msgbuf {
  long mtype;
  char mtext[STRESS_MSGSIZE];  // 修复：使用STRESS_MSGSIZE确保足够大
};

// 新测试函数声明
void test_variable_size_messages();
void test_mixed_type_messages();
void test_nonblocking_mode();
void test_queue_capacity();
void test_message_ordering();

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
  printf("\n=== 高级消息队列压力测试 ===\n");
  
  // 测试1: 不同大小的消息
  printf("\n--- 测试1: 不同大小的消息 ---\n");
  test_variable_size_messages();
  
  // 测试2: 混合类型的消息
  printf("\n--- 测试2: 混合类型的消息 ---\n");
  test_mixed_type_messages();
  
  // 测试3: 非阻塞模式
  printf("\n--- 测试3: 非阻塞模式 ---\n");
  test_nonblocking_mode();
  
  // 测试4: 消息队列容量限制
  printf("\n--- 测试4: 消息队列容量限制 ---\n");
  test_queue_capacity();
  
  // 测试5: 消息顺序性
  printf("\n--- 测试5: 消息顺序性 ---\n");
  test_message_ordering();
  
  printf("\n高级压力测试全部完成！\n");
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


// 测试不同大小的消息
void
test_variable_size_messages()
{
  int msgid;
  struct msgbuf msg;

  // 创建消息队列
  msgid = msgget(0x5000, 0x01000); // IPC_CREAT
  if (msgid < 0) {
    printf("msgget 失败\n");
    exit(1);
  }
  printf("msgid = %d\n", msgid);

  // 发送不同大小的消息
  int sizes[] = {8, 32, 64, 128, 256, 512};
  int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

  for (int i = 0; i < num_sizes; i++) {
    // 发送消息
    msg.mtype = i + 1;
    memset(msg.mtext, 'A' + i, sizes[i]);
    msg.mtext[sizes[i] - 1] = '\0';

    printf("发送大小为 %d 的消息，类型 %d\n", sizes[i], (int)msg.mtype);
    if (msgsnd(msgid, &msg, sizes[i], 0) < 0) {
      printf("msgsnd 失败，消息大小 %d\n", sizes[i]);
      exit(1);
    }
  }

  // 接收消息
  for (int i = 0; i < num_sizes; i++) {
    memset(&msg, 0, sizeof(msg));
    int len = msgrcv(msgid, &msg, sizeof(msg.mtext), i + 1, 0);
    if (len < 0) {
      printf("msgrcv 失败，消息类型 %d\n", i + 1);
      exit(1);
    }

    // 验证消息大小
    if (len != sizes[i]) {
      printf("消息大小不匹配，期望 %d，实际 %d\n", sizes[i], len);
      exit(1);
    }

    // 验证消息内容
    for (int j = 0; j < len - 1; j++) {
      if (msg.mtext[j] != 'A' + i) {
        printf("消息内容不匹配，位置 %d，期望 %c，实际 %c\n", j, 'A' + i, msg.mtext[j]);
        exit(1);
      }
    }

    printf("成功接收大小为 %d 的消息，类型 %d\n", len, (int)msg.mtype);
  }

  // 删除消息队列
  if (msgctl(msgid, 1, 0) < 0) { // IPC_RMID
    printf("msgctl 删除消息队列失败\n");
    exit(1);
  }

  printf("不同大小消息测试通过\n");
}

// 测试混合类型的消息
void
test_mixed_type_messages()
{
  int msgid;
  struct msgbuf msg;

  // 创建消息队列
  msgid = msgget(0x5001, 0x01000); // IPC_CREAT
  if (msgid < 0) {
    printf("msgget 失败\n");
    exit(1);
  }
  printf("msgid = %d\n", msgid);

  // 发送不同类型的消息
  int types[] = {1, 3, 5, 7, 9};
  int num_types = sizeof(types) / sizeof(types[0]);

  for (int i = 0; i < num_types; i++) {
    msg.mtype = types[i];
    strcpy(msg.mtext, "message");
    msg.mtext[7] = '0' + i;
    msg.mtext[8] = '\0';

    printf("发送类型为 %d 的消息\n", types[i]);
    if (msgsnd(msgid, &msg, strlen(msg.mtext), 0) < 0) {
      printf("msgsnd 失败，消息类型 %d\n", types[i]);
      exit(1);
    }
  }

  // 接收特定类型的消息
  for (int i = 0; i < num_types; i++) {
    memset(&msg, 0, sizeof(msg));
    int len = msgrcv(msgid, &msg, sizeof(msg.mtext), types[i], 0);
    if (len < 0) {
      printf("msgrcv 失败，消息类型 %d\n", types[i]);
      exit(1);
    }

    // 验证消息类型
    if (msg.mtype != types[i]) {
      printf("消息类型不匹配，期望 %d，实际 %d\n", types[i], (int)msg.mtype);
      exit(1);
    }

    printf("成功接收类型为 %d 的消息: %s\n", (int)msg.mtype, msg.mtext);
  }

  // 测试接收任意类型的消息 (msgtyp = 0)
  // 先发送几条消息
  for (int i = 0; i < 3; i++) {
    msg.mtype = 10 + i;
    strcpy(msg.mtext, "any_type");
    msg.mtext[8] = '0' + i;
    msg.mtext[9] = '\0';

    if (msgsnd(msgid, &msg, strlen(msg.mtext), 0) < 0) {
      printf("msgsnd 失败，消息类型 %d\n", (int)msg.mtype);
      exit(1);
    }
  }

  // 接收任意类型的消息
  for (int i = 0; i < 3; i++) {
    memset(&msg, 0, sizeof(msg));
    int len = msgrcv(msgid, &msg, sizeof(msg.mtext), 0, 0);
    if (len < 0) {
      printf("msgrcv 失败，接收任意类型消息\n");
      exit(1);
    }

    printf("成功接收任意类型消息，类型 %d: %s\n", (int)msg.mtype, msg.mtext);
  }

  // 测试接收小于等于指定类型的消息 (msgtyp < 0)
  // 先发送几条消息
  for (int i = 0; i < 3; i++) {
    msg.mtype = 20 + i;
    strcpy(msg.mtext, "less_type");
    msg.mtext[9] = '0' + i;
    msg.mtext[10] = '\0';

    if (msgsnd(msgid, &msg, strlen(msg.mtext), 0) < 0) {
      printf("msgsnd 失败，消息类型 %d\n", (int)msg.mtype);
      exit(1);
    }
  }

  // 接收小于等于21的消息
  for (int i = 0; i < 2; i++) {
    memset(&msg, 0, sizeof(msg));
    int len = msgrcv(msgid, &msg, sizeof(msg.mtext), -21, 0);
    if (len < 0) {
      printf("msgrcv 失败，接收小于等于21类型的消息\n");
      exit(1);
    }

    if (msg.mtype > 21) {
      printf("接收到类型大于21的消息: %d\n", (int)msg.mtype);
      exit(1);
    }

    printf("成功接收小于等于21类型的消息，类型 %d: %s\n", (int)msg.mtype, msg.mtext);
  }

  // 删除消息队列
  if (msgctl(msgid, 1, 0) < 0) { // IPC_RMID
    printf("msgctl 删除消息队列失败\n");
    exit(1);
  }

  printf("混合类型消息测试通过\n");
}

// 测试非阻塞模式
void
test_nonblocking_mode()
{
  int msgid;
  struct msgbuf msg;

  // 创建消息队列
  msgid = msgget(0x5002, 0x01000); // IPC_CREAT
  if (msgid < 0) {
    printf("msgget 失败\n");
    exit(1);
  }
  printf("msgid = %d\n", msgid);

  // 测试非阻塞接收空队列
  memset(&msg, 0, sizeof(msg));
  int len = msgrcv(msgid, &msg, sizeof(msg.mtext), 1, 0x08000); // IPC_NOWAIT
  if (len >= 0) {
    printf("非阻塞接收空队列应该失败，但成功了\n");
    exit(1);
  }
  printf("非阻塞接收空队列正确失败\n");

  // 发送一条消息
  msg.mtype = 1;
  strcpy(msg.mtext, "nonblocking");
  if (msgsnd(msgid, &msg, strlen(msg.mtext), 0) < 0) {
    printf("msgsnd 失败\n");
    exit(1);
  }

  // 测试非阻塞接收有消息的队列
  memset(&msg, 0, sizeof(msg));
  len = msgrcv(msgid, &msg, sizeof(msg.mtext), 1, 0x08000); // IPC_NOWAIT
  if (len < 0) {
    printf("非阻塞接收有消息的队列失败\n");
    exit(1);
  }
  printf("非阻塞接收有消息的队列成功: %s\n", msg.mtext);

  // 测试非阻塞发送满队列
  // 填满队列
  for (int i = 0; i < 16; i++) { // MAX_MSG_QUEUE_SIZE = 16
    msg.mtype = 1;
    strcpy(msg.mtext, "fill_queue");
    if (msgsnd(msgid, &msg, strlen(msg.mtext), 0) < 0) {
      printf("填充队列失败在消息 %d\n", i);
      exit(1);
    }
  }

  // 尝试向满队列发送消息（非阻塞）
  msg.mtype = 1;
  strcpy(msg.mtext, "overflow");
  if (msgsnd(msgid, &msg, strlen(msg.mtext), 0x08000) >= 0) { // IPC_NOWAIT
    printf("非阻塞发送满队列应该失败，但成功了\n");
    exit(1);
  }
  printf("非阻塞发送满队列正确失败\n");

  // 删除消息队列
  if (msgctl(msgid, 1, 0) < 0) { // IPC_RMID
    printf("msgctl 删除消息队列失败\n");
    exit(1);
  }

  printf("非阻塞模式测试通过\n");
}

// 测试消息队列容量限制
void
test_queue_capacity()
{
  int msgid;
  struct msgbuf msg;

  // 创建消息队列
  msgid = msgget(0x5003, 0x01000); // IPC_CREAT
  if (msgid < 0) {
    printf("msgget 失败\n");
    exit(1);
  }
  printf("msgid = %d\n", msgid);

  // 填满队列
  int count = 0;
  for (int i = 0; i < 16; i++) { // MAX_MSG_QUEUE_SIZE = 16
    msg.mtype = 1;
    sprintf(msg.mtext, "message_%d", i);
    if (msgsnd(msgid, &msg, strlen(msg.mtext), 0) < 0) {
      printf("填充队列失败在消息 %d\n", i);
      exit(1);
    }
    count++;
  }
  printf("成功填充队列，共 %d 条消息\n", count);

  // 尝试添加第17条消息（应该阻塞）
  int pid = fork();
  if (pid < 0) {
    printf("fork 失败\n");
    exit(1);
  }

  if (pid == 0) {
    // 子进程：尝试发送第17条消息
    msg.mtype = 1;
    strcpy(msg.mtext, "overflow_message");
    printf("子进程尝试发送第17条消息（应该阻塞）\n");
    if (msgsnd(msgid, &msg, strlen(msg.mtext), 0) < 0) {
      printf("子进程发送消息失败\n");
      exit(1);
    }
    printf("子进程成功发送第17条消息\n");
    exit(0);
  } else {
    // 父进程：等待一会儿，然后接收一条消息
    sleep(1);
    memset(&msg, 0, sizeof(msg));
    int len = msgrcv(msgid, &msg, sizeof(msg.mtext), 0, 0);
    if (len < 0) {
      printf("父进程接收消息失败\n");
      exit(1);
    }
    printf("父进程接收消息: %s\n", msg.mtext);

    // 等待子进程结束
    int status;
    wait(&status);
    if (status != 0) {
      printf("子进程异常退出，状态 %d\n", status);
      exit(1);
    }
  }

  // 删除消息队列
  if (msgctl(msgid, 1, 0) < 0) { // IPC_RMID
    printf("msgctl 删除消息队列失败\n");
    exit(1);
  }

  printf("消息队列容量限制测试通过\n");
}

// 测试消息顺序性
void
test_message_ordering()
{
  int msgid;
  struct msgbuf msg;

  // 创建消息队列
  msgid = msgget(0x5004, 0x01000); // IPC_CREAT
  if (msgid < 0) {
    printf("msgget 失败\n");
    exit(1);
  }
  printf("msgid = %d\n", msgid);

  // 发送多条相同类型的消息
  for (int i = 0; i < 10; i++) {
    msg.mtype = 1;
    sprintf(msg.mtext, "message_%d", i);
    if (msgsnd(msgid, &msg, strlen(msg.mtext), 0) < 0) {
      printf("msgsnd 失败，消息 %d\n", i);
      exit(1);
    }
  }

  // 接收消息并验证顺序
  for (int i = 0; i < 10; i++) {
    memset(&msg, 0, sizeof(msg));
    int len = msgrcv(msgid, &msg, sizeof(msg.mtext), 1, 0);
    if (len < 0) {
      printf("msgrcv 失败，消息 %d\n", i);
      exit(1);
    }

    // 验证消息顺序
    char expected[16];
    sprintf(expected, "message_%d", i);
    if (strcmp(msg.mtext, expected) != 0) {
      printf("消息顺序不正确，期望 %s，实际 %s\n", expected, msg.mtext);
      exit(1);
    }

    printf("成功接收消息 %d: %s\n", i, msg.mtext);
  }

  // 测试不同类型消息的顺序
  // 先发送不同类型的消息
  for (int i = 0; i < 5; i++) {
    msg.mtype = i + 1;
    sprintf(msg.mtext, "type_%d_msg_%d", i + 1, i + 1);
    if (msgsnd(msgid, &msg, strlen(msg.mtext), 0) < 0) {
      printf("msgsnd 失败，消息 %d\n", i);
      exit(1);
    }
  }

  // 按类型顺序接收
  for (int i = 0; i < 5; i++) {
    memset(&msg, 0, sizeof(msg));
    int len = msgrcv(msgid, &msg, sizeof(msg.mtext), i + 1, 0);
    if (len < 0) {
      printf("msgrcv 失败，类型 %d\n", i + 1);
      exit(1);
    }

    // 验证消息类型
    if (msg.mtype != i + 1) {
      printf("消息类型不正确，期望 %d，实际 %d\n", i + 1, (int)msg.mtype);
      exit(1);
    }

    printf("成功接收类型 %d 消息: %s\n", (int)msg.mtype, msg.mtext);
  }

  // 删除消息队列
  if (msgctl(msgid, 1, 0) < 0) { // IPC_RMID
    printf("msgctl 删除消息队列失败\n");
    exit(1);
  }

  printf("消息顺序性测试通过\n");
}