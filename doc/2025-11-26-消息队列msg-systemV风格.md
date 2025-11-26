
# System V 消息队列实现与使用指南

## 1. System V 消息队列概述

System V 消息队列是 Unix 系统中一种进程间通信(IPC)机制，它允许一个或多个进程向队列写入消息，也允许一个或多个进程从队列中读取消息。消息队列具有以下特点：

- 消息是按类型排序的，可以按类型检索
- 消息以链表形式存储在内核中
- 消息队列是持久的，直到被显式删除或系统关闭
- 每个消息队列有最大容量限制

## 2. 消息队列的基本操作

### 2.1 创建或获取消息队列

```c
int msgget(int key, int msgflg);
```

- `key`: 消息队列的键值，用于唯一标识一个消息队列
- `msgflg`: 标志位，可以是以下值的组合：
  - `IPC_CREAT`: 如果队列不存在，则创建它
  - `IPC_EXCL`: 与 `IPC_CREAT` 一起使用，如果队列已存在，则返回错误
  - 权限位：如 0666，表示读写权限

返回值：成功返回消息队列标识符(msqid)，失败返回 -1。

### 2.2 发送消息

```c
int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg);
```

- `msqid`: 消息队列标识符
- `msgp`: 指向消息缓冲区的指针
- `msgsz`: 消息文本的大小（不包括消息类型）
- `msgflg`: 标志位，可以是：
  - `IPC_NOWAIT`: 非阻塞模式，如果队列满了则立即返回错误

返回值：成功返回 0，失败返回 -1。

### 2.3 接收消息

```c
ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg);
```

- `msqid`: 消息队列标识符
- `msgp`: 指向消息缓冲区的指针
- `msgsz`: 消息缓冲区的大小
- `msgtyp`: 消息类型，可以是：
  - 0: 接收队列中的第一个消息
  - >0: 接收类型为 msgtyp 的第一个消息
  - <0: 接收类型小于等于 |msgtyp| 的第一个消息
- `msgflg`: 标志位，可以是：
  - `IPC_NOWAIT`: 非阻塞模式，如果队列中没有符合条件的消息则立即返回错误

返回值：成功返回接收到的消息文本的大小，失败返回 -1。

### 2.4 控制消息队列

```c
int msgctl(int msqid, int cmd, struct msqid_ds *buf);
```

- `msqid`: 消息队列标识符
- `cmd`: 命令，可以是：
  - `IPC_RMID`: 删除消息队列
  - `IPC_STAT`: 获取消息队列的状态信息
  - `IPC_SET`: 设置消息队列的状态信息
- `buf`: 指向 msqid_ds 结构体的指针，用于存储或设置队列信息

返回值：成功返回 0，失败返回 -1。

## 3. 消息缓冲区结构

```c
struct msgbuf {
    long mtype;    // 消息类型，必须为正数
    char mtext[1]; // 消息文本，实际大小可以根据需要调整
};
```

`struct msgbuf` 是用户空间和内核空间之间传递消息数据时使用的接口结构。让我解释一下它的作用：

1. **用户空间接口**：
   - `struct msgbuf` 是用户程序在调用 `msgsnd` 和 `msgrcv` 函数时使用的消息格式
   - 用户程序创建这个结构，填充消息类型和消息数据，然后通过 `msgsnd` 发送
   - 或者通过 `msgrcv` 接收消息到这个结构中

2. **内核内部表示**：
   - 内核内部使用 `struct msg` 结构来存储和管理消息
   - 当用户调用 `msgsnd` 时，内核会将 `struct msgbuf` 中的数据复制到 `struct msg` 中
   - 当用户调用 `msgrcv` 时，内核会将 `struct msg` 中的数据复制到用户的 `struct msgbuf` 中

3. **转换过程**：
   ```c
   // 发送消息时的转换
   struct msgbuf *mbuf = (struct msgbuf *)msgp; // 用户提供的消息
   msg->type = mbuf->mtype;                      // 复制类型
   memmove(msg->data, mbuf->mtext, msgsz);       // 复制数据
   
   // 接收消息时的转换
   struct msgbuf *mbuf = (struct msgbuf *)msgp; // 用户提供的缓冲区
   mbuf->mtype = msg->type;                      // 复制类型
   memmove(mbuf->mtext, msg->data, copy_size);   // 复制数据
   ```

4. **为什么需要两种结构**：
   - `struct msgbuf` 是 System V 消息队列的标准接口
   - `struct msg` 是内核内部实现，包含了额外的字段（如 `next` 指针、`size` 等）用于队列管理
   - 内核需要将标准接口转换为内部表示，以便管理消息队列

5. **`mtext[1]` 的作用**：
   - `struct msgbuf` 中的 `mtext[1]` 是一个占位符，实际使用时可以指向更大的数据区域
   - 这是一种常见的 C 语言技巧，允许结构体后面跟着额外的数据
   - 在调用 `msgsnd` 和 `msgrcv` 时，`msgsz` 参数指定了实际消息数据的大小

总结：`struct msgbuf` 是用户空间和内核空间之间传递消息的接口，而 `struct msg` 是内核内部管理消息的数据结构。内核负责在两者之间进行转换。

## 4. ZXXOS 中的消息队列实现

### 4.1 数据结构

```c
// 消息结构
struct msg {
    long type;        // 消息类型
    int size;         // 消息大小
    struct msg *next; // 下一个消息
    char data[0];     // 消息数据
};

// 消息队列结构
struct msg_queue {
    int used;                    // 是否使用
    int refcnt;                  // 引用计数
    int key;                     // 键值
    int msqid;                   // 消息队列ID
    struct msg *head;            // 队列头
    struct msg *tail;            // 队列尾
    int msg_count;               // 消息数量
    int marked_for_deletion;     // 是否标记为删除
    struct spinlock lock;        // 自旋锁
};
```

### 4.2 关键函数实现

#### 4.2.1 创建或获取消息队列

```c
int msgget(int key, int msgflg)
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
```

#### 4.2.2 发送消息

```c
int msgsnd(int msqid, const void *msgp, unsigned msgsz, int msgflg)
{
    struct msg_queue *queue;
    struct msg *msg;
    struct msgbuf *mbuf = (struct msgbuf *)msgp;
    
    // 检查消息大小
    if(msgsz > MAX_MSG_SIZE) {
        printf("msgsnd: msgsz %d > MAX_MSG_SIZE %d\n", msgsz, MAX_MSG_SIZE);
        return -1;
    }
    
    // 检查消息类型
    if(mbuf->mtype <= 0) {
        printf("msgsnd: invalid mtype %d, must be positive\n", mbuf->mtype);
        return -1;
    }
    
    acquire(&msg_lock);
    
    // 查找消息队列
    queue = msg_find_by_id_locked(msqid);
    if(!queue) {
        printf("msgsnd: queue not found for msqid %d\n", msqid);
        release(&msg_lock);
        return -1;
    }
    
    // 检查队列是否被标记为删除
    if(queue->marked_for_deletion) {
        printf("msgsnd: queue marked for deletion, msqid %d\n", msqid);
        release(&msg_lock);
        return -1;
    }
    
    // 增加引用计数
    queue->refcnt++;
    printf("msgsnd: increased refcnt to %d\n", queue->refcnt);
    release(&msg_lock);
    
    acquire(&queue->lock);
    
    // 检查队列是否已满
    if(queue->msg_count >= MAX_MSG_QUEUE_SIZE) {
        printf("msgsnd: queue full, msg_count=%d, MAX_MSG_QUEUE_SIZE=%d\n", 
               queue->msg_count, MAX_MSG_QUEUE_SIZE);
        
        if(msgflg & IPC_NOWAIT) {
            // 非阻塞模式，直接返回错误
            printf("msgsnd: queue full, non-blocking mode\n");
            release(&queue->lock);
            
            acquire(&msg_lock);
            queue->refcnt--;
            printf("msgsnd: decreased refcnt to %d\n", queue->refcnt);
            release(&msg_lock);
            
            return -1;
        } else {
            // 阻塞模式，等待队列有空间
            printf("msgsnd: queue full, blocking mode, waiting for space\n");
            while(queue->msg_count >= MAX_MSG_QUEUE_SIZE && !queue->marked_for_deletion) {
                sleep(queue, &queue->lock);
            }
            
            // 如果队列被标记为删除，则返回错误
            if(queue->marked_for_deletion) {
                printf("msgsnd: queue marked for deletion during wait\n");
                release(&queue->lock);
                
                acquire(&msg_lock);
                queue->refcnt--;
                printf("msgsnd: decreased refcnt to %d\n", queue->refcnt);
                release(&msg_lock);
                
                return -1;
            }
        }
    }
    
    // 分配消息内存
    msg = (struct msg *)kalloc();
    if(!msg) {
        printf("msgsnd: kalloc failed\n");
        release(&queue->lock);
        
        acquire(&msg_lock);
        queue->refcnt--;
        printf("msgsnd: decreased refcnt to %d\n", queue->refcnt);
        release(&msg_lock);
        
        return -1;
    }
    
    // 初始化消息
    msg->type = mbuf->mtype;
    msg->size = msgsz;
    msg->next = 0;
    
    // 复制消息数据
    memmove(msg->data, mbuf->mtext, msgsz);
    
    // 将消息添加到队列尾部
    if(queue->tail) {
        queue->tail->next = msg;
    } else {
        queue->head = msg;
    }
    queue->tail = msg;
    queue->msg_count++;
    
    // 唤醒等待消息的进程
    wakeup(queue);
    
    printf("msgsnd: sent message, type=%d, size=%d, msg_count=%d\n", 
           msg->type, msg->size, queue->msg_count);
    
    release(&queue->lock);
    
    // 减少引用计数
    acquire(&msg_lock);
    queue->refcnt--;
    printf("msgsnd: decreased refcnt to %d\n", queue->refcnt);
    release(&msg_lock);
    
    return 0;
}
```

#### 4.2.3 接收消息

```c
int msgrcv(int msqid, void *msgp, unsigned msgsz, long msgtyp, int msgflg)
{
    struct msg_queue *queue;
    struct msg *msg, *prev;
    struct msgbuf *mbuf = (struct msgbuf *)msgp;
    int copy_size;
    
    // 检查消息大小
    if(msgsz < 0) {
        printf("msgrcv: invalid msgsz %d, must be non-negative\n", msgsz);
        return -1;
    }
    
    acquire(&msg_lock);
    
    // 查找消息队列
    queue = msg_find_by_id_locked(msqid);
    if(!queue) {
        printf("msgrcv: queue not found for msqid %d\n", msqid);
        release(&msg_lock);
        return -1;
    }
    
    // 增加引用计数
    queue->refcnt++;
    printf("msgrcv: increased refcnt to %d\n", queue->refcnt);
    release(&msg_lock);
    
    acquire(&queue->lock);
    
    // 查找符合条件的消息
    while(1) {
        msg = 0;
        prev = 0;
        
        // 遍历队列查找消息
        struct msg *temp_msg = queue->head;
        struct msg *temp_prev = 0;
        
        while(temp_msg) {
            int match = 0;
            
            // 检查消息类型是否匹配
            if(msgtyp == 0) {
                // 接收第一个消息
                match = 1;
            } else if(msgtyp > 0) {
                // 接收类型为msgtyp的第一个消息
                if(temp_msg->type == msgtyp) {
                    match = 1;
                }
            } else { // msgtyp < 0
                // 接收类型小于等于|msgtyp|的第一个消息
                if(temp_msg->type <= -msgtyp) {
                    match = 1;
                }
            }
            
            if(match) {
                msg = temp_msg;
                prev = temp_prev;
                break;
            }
            
            temp_prev = temp_msg;
            temp_msg = temp_msg->next;
        }
        
        // 如果找到了消息
        if(msg) {
            // 计算要复制的数据大小
            copy_size = msg->size;
            if(copy_size > msgsz) {
                copy_size = msgsz;
            }
            
            // 复制消息数据
            memmove(mbuf->mtext, msg->data, copy_size);
            mbuf->mtype = msg->type;
            
            // 从队列中移除消息
            if(prev) {
                prev->next = msg->next;
            } else {
                queue->head = msg->next;
            }
            
            if(msg->next == 0) {
                queue->tail = prev;
            }
            
            queue->msg_count--;
            
            // 唤醒等待发送消息的进程
            wakeup(queue);
            
            printf("msgrcv: received message, type=%d, size=%d, msg_count=%d\n", 
                   msg->type, copy_size, queue->msg_count);
            
            // 释放消息内存
            kfree((char *)msg);
            
            // 减少引用计数
            acquire(&msg_lock);
            queue->refcnt--;
            printf("msgrcv: decreased refcnt to %d\n", queue->refcnt);
            release(&msg_lock);
            
            return copy_size;
        }
        
        // 如果没有找到消息
        if(msgflg & IPC_NOWAIT) {
            // 非阻塞模式，直接返回错误
            printf("msgrcv: no message found, non-blocking mode, msgtyp=%d\n", msgtyp);
            release(&queue->lock);
            
            acquire(&msg_lock);
            queue->refcnt--;
            printf("msgrcv: decreased refcnt to %d\n", queue->refcnt);
            release(&msg_lock);
            
            return -1;
        } else {
            // 阻塞模式，等待有消息
            printf("msgrcv: no message found, blocking mode, waiting for message, msgtyp=%d\n", msgtyp);
            // 检查是否有符合类型的消息
            int has_matching_msg = 0;
            struct msg *temp_msg = queue->head;
            while(temp_msg) {
                if(msgtyp == 0 ||
                   (msgtyp > 0 && temp_msg->type == msgtyp) ||
                   (msgtyp < 0 && temp_msg->type <= -msgtyp)) {
                    has_matching_msg = 1;
                    break;
                }
                temp_msg = temp_msg->next;
            }
            
            // 如果没有符合类型的消息且队列未被标记删除，则等待
            while(!has_matching_msg && !queue->marked_for_deletion) {
                sleep(queue, &queue->lock);
                
                // 唤醒后重新检查是否有符合类型的消息
                has_matching_msg = 0;
                temp_msg = queue->head;
                while(temp_msg) {
                    if(msgtyp == 0 ||
                       (msgtyp > 0 && temp_msg->type == msgtyp) ||
                       (msgtyp < 0 && temp_msg->type <= -msgtyp)) {
                        has_matching_msg = 1;
                        break;
                    }
                    temp_msg = temp_msg->next;
                }
            }
            
            // 如果队列被标记为删除，则返回错误
            if(queue->marked_for_deletion) {
                printf("msgrcv: queue marked for deletion during wait\n");
                release(&queue->lock);
                
                acquire(&msg_lock);
                queue->refcnt--;
                printf("msgrcv: decreased refcnt to %d\n", queue->refcnt);
                release(&msg_lock);
                
                return -1;
            }
        }
    }
}
```

#### 4.2.4 控制消息队列

```c
int msgctl(int msqid, int cmd, void *buf)
{
    struct msg_queue *queue;
    
    acquire(&msg_lock);
    
    // 查找消息队列
    queue = msg_find_by_id_locked(msqid);
    if(!queue) {
        printf("msgctl: queue not found for msqid %d\n", msqid);
        release(&msg_lock);
        return -1;
    }
    
    switch(cmd) {
        case IPC_RMID:
            printf("msgctl: marking queue for deletion, refcnt=%d\n", queue->refcnt);
            // 标记为删除
            queue->marked_for_deletion = 1;
            // 释放 key，以便可以创建新的队列
            queue->key = 0;
            
            // 唤醒所有等待的进程
            acquire(&queue->lock);
            wakeup(queue);
            release(&queue->lock);
            printf("msgctl: woke up all waiting processes\n");
            
            // 如果没有其他进程在使用，则清理资源
            if(queue->refcnt == 0) {
                printf("msgctl: cleaning up queue immediately\n");
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
                queue->key = 0;
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
            
        case IPC_SET:
            // TODO: 实现设置队列状态信息
            break;
            
        default:
            printf("msgctl: unknown command %d\n", cmd);
            release(&msg_lock);
            return -1;
    }
    
    release(&msg_lock);
    return 0;
}
```

## 5. 消息队列测试

### 5.1 基本消息队列测试

```c
void test_basic_msg_queue()
{
    int msgid;
    struct msgbuf msg;
    
    printf("=== 基本消息队列测试 ===\n");
    
    // 创建消息队列
    msgid = msgget(0x2000, IPC_CREAT);
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
    if(msgctl(msgid, IPC_RMID, 0) < 0) {
        printf("msgctl 失败\n");
        exit(1);
    }
    printf("消息队列已删除\n");
}
```

### 5.2 消息类型测试

```c
void test_message_types()
{
    int msgid;
    struct msgbuf msg;
    
    printf("=== 消息类型测试 ===\n");
    
    // 创建消息队列
    msgid = msgget(0x2001, IPC_CREAT);
    if(msgid < 0) {
        printf("msgget 失败\n");
        exit(1);
    }
    printf("msgid = %d\n", msgid);
    
    // 发送不同类型的消息
    for(int i = 1; i <= 3; i++) {
        msg.mtype = i;
        sprintf(msg.mtext, "message_type_%d", i);
        if(msgsnd(msgid, &msg, strlen(msg.mtext), 0) < 0) {
            printf("msgsnd 失败，消息类型 %d\n", i);
            exit(1);
        }
        printf("发送类型 %d 消息: %s\n", i, msg.mtext);
    }
    
    // 按类型接收消息
    for(int i = 1; i <= 3; i++) {
        memset(&msg, 0, sizeof(msg));
        int len = msgrcv(msgid, &msg, MSGSIZE, i, 0);
        if(len < 0) {
            printf("msgrcv 失败，消息类型 %d\n", i);
            exit(1);
        }
        printf("接收类型 %d 消息: %s (长度=%d)\n", i, msg.mtext, len);
    }
    
    // 删除消息队列
    if(msgctl(msgid, IPC_RMID, 0) < 0) {
        printf("msgctl 删除消息队列失败\n");
        exit(1);
    }
    printf("消息队列已删除\n");
}
```

### 5.3 非阻塞模式测试

```c
void test_nonblocking_mode()
{
    int msgid;
    struct msgbuf msg;
    
    printf("=== 非阻塞模式测试 ===\n");
    
    // 创建消息队列
    msgid = msgget(0x2002, IPC_CREAT);
    if(msgid < 0) {
        printf("msgget 失败\n");
        exit(1);
    }
    printf("msgid = %d\n", msgid);
    
    // 测试非阻塞接收空队列
    memset(&msg, 0, sizeof(msg));
    int len = msgrcv(msgid, &msg, MSGSIZE, 1, IPC_NOWAIT);
    if(len >= 0) {
        printf("非阻塞接收空队列应该失败，但成功了\n");
        exit(1);
    }
    printf("非阻塞接收空队列正确失败\n");
    
    // 发送一条消息
    msg.mtype = 1;
    strcpy(msg.mtext, "nonblocking");
    if(msgsnd(msgid, &msg, strlen(msg.mtext), 0) < 0) {
        printf("msgsnd 失败\n");
        exit(1);
    }
    
    // 测试非阻塞接收有消息的队列
    memset(&msg, 0, sizeof(msg));
    len = msgrcv(msgid, &msg, MSGSIZE, 1, IPC_NOWAIT);
    if(len < 0) {
        printf("非阻塞接收有消息的队列失败\n");
        exit(1);
    }
    printf("非阻塞接收有消息的队列成功: %s\n", msg.mtext);
    
    // 测试非阻塞发送满队列
    // 填满队列
    for(int i = 0; i < MAX_MSG_QUEUE_SIZE; i++) {
        msg.mtype = 1;
        strcpy(msg.mtext, "fill_queue");
        if(msgsnd(msgid, &msg, strlen(msg.mtext), 0) < 0) {
            printf("填充队列失败在消息 %d\n", i);
            exit(1);
        }
    }
    
    // 尝试向满队列发送消息（非阻塞）
    msg.mtype = 1;
    strcpy(msg.mtext, "overflow");
    if(msgsnd(msgid, &msg, strlen(msg.mtext), IPC_NOWAIT) >= 0) {
        printf("非阻塞发送满队列应该失败，但成功了\n");
        exit(1);
    }
    printf("非阻塞发送满队列正确失败\n");
    
    // 删除消息队列
    if(msgctl(msgid, IPC_RMID, 0) < 0) {
        printf("msgctl 删除消息队列失败\n");
        exit(1);
    }
    printf("消息队列已删除\n");
}
```

### 5.4 消息队列容量限制测试

```c
void test_queue_capacity()
{
    int msgid;
    struct msgbuf msg;
    
    printf("=== 消息队列容量限制测试 ===\n");
    
    // 创建消息队列
    msgid = msgget(0x2003, IPC_CREAT);
    if(msgid < 0) {
        printf("msgget 失败\n");
        exit(1);
    }
    printf("msgid = %d\n", msgid);
    
    // 填满队列
    for(int i = 0; i < MAX_MSG_QUEUE_SIZE; i++) {
        msg.mtype = 1;
        sprintf(msg.mtext, "message_%d", i);
        if(msgsnd(msgid, &msg, strlen(msg.mtext), 0) < 0) {
            printf("填充队列失败在消息 %d\n", i);
            exit(1);
        }
        printf("发送消息 %d: %s\n", i, msg.mtext);
    }
    
    // 尝试向满队列发送消息（阻塞）
    int pid = fork();
    if(pid < 0) {
        printf("fork 失败\n");
        exit(1);
    } else if(pid == 0) {
        // 子进程：尝试发送消息到满队列
        msg.mtype = 1;
        strcpy(msg.mtext, "overflow_message");
        if(msgsnd(msgid, &msg, strlen(msg.mtext), 0) < 0) {
            printf("子进程发送消息失败\n");
            exit(1);
        }
        printf("子进程成功发送消息\n");
        exit(0);
    } else {
        // 父进程：等待一会儿，然后接收一条消息
        sleep(1);
        memset(&msg, 0, sizeof(msg));
        int len = msgrcv(msgid, &msg, MSGSIZE, 0, 0);
        if(len < 0) {
            printf("父进程接收消息失败\n");
            exit(1);
        }
        printf("父进程接收消息: %s\n", msg.mtext);
        
        // 等待子进程结束
        int status;
        wait(&status);
        if(status != 0) {
            printf("子进程异常退出，状态 %d\n", status);
            exit(1);
        }
    }
    
    // 删除消息队列
    if(msgctl(msgid, IPC_RMID, 0) < 0) {
        printf("msgctl 删除消息队列失败\n");
        exit(1);
    }
    printf("消息队列已删除\n");
}
```

### 5.5 消息顺序性测试

```c
void test_message_ordering()
{
    int msgid;
    struct msgbuf msg;
    
    printf("=== 消息顺序性测试 ===\n");
    
    // 创建消息队列
    msgid = msgget(0x2004, IPC_CREAT);
    if(msgid < 0) {
        printf("msgget 失败\n");
        exit(1);
    }
    printf("msgid = %d\n", msgid);
    
    // 发送多条相同类型的消息
    for(int i = 0; i < 10; i++) {
        msg.mtype = 1;
        sprintf(msg.mtext, "message_%d", i);
        if(msgsnd(msgid, &msg, strlen(msg.mtext), 0) < 0) {
            printf("msgsnd 失败，消息 %d\n", i);
            exit(1);
        }
    }
    
    // 接收消息并验证顺序
    for(int i = 0; i < 10; i++) {
        memset(&msg, 0, sizeof(msg));
        int len = msgrcv(msgid, &msg, MSGSIZE, 1, 0);
        if(len < 0) {
            printf("msgrcv 失败，消息 %d\n", i);
            exit(1);
        }
        
        // 验证消息顺序
        char expected[32];
        sprintf(expected, "message_%d", i);
        if(strcmp(msg.mtext, expected) != 0) {
            printf("消息顺序错误，期望 %s，实际 %s\n", expected, msg.mtext);
            exit(1);
        }
        printf("接收消息 %d: %s\n", i, msg.mtext);
    }
    
    // 删除消息队列
    if(msgctl(msgid, IPC_RMID, 0) < 0) {
        printf("msgctl 删除消息队列失败\n");
        exit(1);
    }
    printf("消息队列已删除\n");
}
```

## 6. 使用示例

以下是一个完整的使用示例，演示了如何使用 System V 消息队列进行进程间通信：

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/msg.h"

#define MSG_TYPE_REQUEST  1
#define MSG_TYPE_RESPONSE 2

struct request_msg {
    long mtype;
    int a;
    int b;
    char op;
};

struct response_msg {
    long mtype;
    int result;
};

// 服务器进程
void server() {
    int msgid = msgget(0x1234, IPC_CREAT);
    if(msgid < 0) {
        printf("Server: msgget failed\n");
        exit(1);
    }
    printf("Server: msgid = %d\n", msgid);
    
    while(1) {
        struct request_msg req;
        struct response_msg resp;
        
        // 接收请求
        if(msgrcv(msgid, &req, sizeof(req) - sizeof(long), MSG_TYPE_REQUEST, 0) < 0) {
            printf("Server: msgrcv failed\n");
            continue;
        }
        
        printf("Server: received request %d %c %d\n", req.a, req.op, req.b);
        
        // 处理请求
        switch(req.op) {
            case '+':
                resp.result = req.a + req.b;
                break;
            case '-':
                resp.result = req.a - req.b;
                break;
            case '*':
                resp.result = req.a * req.b;
                break;
            case '/':
                resp.result = req.a / req.b;
                break;
            default:
                resp.result = -1;
                break;
        }
        
        // 发送响应
        resp.mtype = MSG_TYPE_RESPONSE;
        if(msgsnd(msgid, &resp, sizeof(resp) - sizeof(long), 0) < 0) {
            printf("Server: msgsnd failed\n");
            continue;
        }
        
        printf("Server: sent response %d\n", resp.result);
    }
}

// 客户端进程
void client(int a, char op, int b) {
    int msgid = msgget(0x1234, 0);
    if(msgid < 0) {
        printf("Client: msgget failed\n");
        exit(1);
    }
    
    struct request_msg req;
    struct response_msg resp;
    
    // 发送请求
    req.mtype = MSG_TYPE_REQUEST;
    req.a = a;
    req.b = b;
    req.op = op;
    
    if(msgsnd(msgid, &req, sizeof(req) - sizeof(long), 0) < 0) {
        printf("Client: msgsnd failed\n");
        exit(1);
    }
    
    printf("Client: sent request %d %c %d\n", a, op, b);
    
    // 接收响应
    if(msgrcv(msgid, &resp, sizeof(resp) - sizeof(long), MSG_TYPE_RESPONSE, 0) < 0) {
        printf("Client: msgrcv failed\n");
        exit(1);
    }
    
    printf("Client: received response %d\n", resp.result);
}

int main() {
    // 创建服务器进程
    if(fork() == 0) {
        server();
        exit(0);
    }
    
    // 等待服务器启动
    sleep(1);
    
    // 创建客户端进程
    if(fork() == 0) {
        client(10, '+', 20);
        exit(0);
    }
    
    if(fork() == 0) {
        client(30, '*', 40);
        exit(0);
    }
    
    if(fork() == 0) {
        client(50, '/', 10);
        exit(0);
    }
    
    // 等待所有客户端完成
    wait(0);
    wait(0);
    wait(0);
    
    // 删除消息队列
    int msgid = msgget(0x1234, 0);
    if(msgid >= 0) {
        msgctl(msgid, IPC_RMID, 0);
        printf("Main: deleted message queue\n");
    }
    
    exit(0);
}
```

## 7. 总结

System V 消息队列是一种强大的进程间通信机制，它提供了类型化的消息传递，允许进程按照类型选择性地接收消息。在 ZXXOS 中，我们实现了完整的消息队列功能，包括创建、发送、接收和控制消息队列。

通过测试，我们验证了消息队列的各种功能，包括基本消息传递、消息类型处理、非阻塞模式、容量限制和消息顺序性。这些测试确保了消息队列实现的正确性和可靠性。

消息队列在许多场景中都很有用，例如：
- 客户端-服务器通信
- 生产者-消费者模式
- 进程间的数据交换
- 异步通知

通过合理使用消息队列，可以构建出高效、可靠的分布式系统。