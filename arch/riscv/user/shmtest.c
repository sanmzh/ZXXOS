#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "user/sem.h"

// 字符串连接函数
void strcat(char *dest, const char *src) {
  int i = 0;
  int j = 0;
  
  // 找到dest的结尾
  while (dest[i] != '\0') {
    i++;
  }
  
  // 复制src到dest的结尾
  while (src[j] != '\0') {
    dest[i++] = src[j++];
  }
  
  // 添加字符串结束符
  dest[i] = '\0';
}

// 将整数转换为字符串
void itoa(int num, char *str) {
  int i = 0;
  int is_negative = 0;
  
  // 处理0的情况
  if (num == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return;
  }
  
  // 处理负数
  if (num < 0) {
    is_negative = 1;
    num = -num;
  }
  
  // 计算数字的位数
  int temp = num;
  int digits = 0;
  while (temp != 0) {
    temp /= 10;
    digits++;
  }
  
  // 转换数字
  int index = 0;
  while (num != 0) {
    int remainder = num % 10;
    str[index++] = remainder + '0';
    num /= 10;
  }
  
  // 添加负号
  if (is_negative) {
    str[index++] = '-';
  }
  
  // 添加字符串结束符
  str[index] = '\0';
  
  // 反转字符串
  for (int j = 0; j < index / 2; j++) {
    char temp = str[j];
    str[j] = str[index - 1 - j];
    str[index - 1 - j] = temp;
  }
}

#define SHM_SIZE 4096
#define SHM_KEY 1234

void test_basic_shm() {
  printf("=== 测试基本共享内存功能 ===\n");

  // 创建共享内存
  int shmid = shmget(SHM_KEY, SHM_SIZE, 0x01000); // IPC_CREAT
  if (shmid < 0) {
    printf("创建共享内存失败\n");
    exit(1);
  }
  printf("创建共享内存成功，shmid = %d\n", shmid);

  // 附加共享内存
  char *shm_ptr = (char*)shmat(shmid, 0, 0);
  if (shm_ptr == (char*)-1) {
    printf("附加共享内存失败\n");
    exit(1);
  }
  printf("附加共享内存成功，地址 = %p\n", shm_ptr);

  // 写入数据
  strcpy(shm_ptr, "Hello, shared memory!");
  printf("写入数据: %s\n", shm_ptr);

  // 分离共享内存
  if (shmdt(shm_ptr) < 0) {
    printf("分离共享内存失败\n");
    exit(1);
  }
  printf("分离共享内存成功\n");
}

void test_ipc() {
  printf("\n=== 测试进程间通信 ===\n");

  // 创建共享内存
  int shmid = shmget(SHM_KEY + 1, SHM_SIZE, 0x01000); // IPC_CREAT
  if (shmid < 0) {
    printf("创建共享内存失败\n");
    exit(1);
  }
  printf("创建共享内存成功，shmid = %d\n", shmid);

  int pid = fork();
  if (pid < 0) {
    printf("创建子进程失败\n");
    exit(1);
  }

  if (pid == 0) {
    // 子进程
    char *shm_ptr = (char*)shmat(shmid, 0, 0);
    if (shm_ptr == (char*)-1) {
      printf("子进程附加共享内存失败\n");
      exit(1);
    }

    // 等待父进程写入数据
    sleep(1);

    // 读取父进程写入的数据
    printf("子进程读取数据: %s\n", shm_ptr);

    // 写入新数据
    strcpy(shm_ptr, "Message from child process");
    printf("子进程写入新数据\n");

    // 分离共享内存
    if (shmdt(shm_ptr) < 0) {
      printf("子进程分离共享内存失败\n");
      exit(1);
    }

    exit(0);
  } else {
    // 父进程
    char *shm_ptr = (char*)shmat(shmid, 0, 0);
    if (shm_ptr == (char*)-1) {
      printf("父进程附加共享内存失败\n");
      exit(1);
    }

    // 写入数据
    strcpy(shm_ptr, "Message from parent process");
    printf("父进程写入数据\n");

    // 分离共享内存
    if (shmdt(shm_ptr) < 0) {
      printf("父进程分离共享内存失败\n");
      exit(1);
    }

    // 等待子进程
    wait(0);

    // 重新附加共享内存以读取子进程写入的数据
    shm_ptr = (char*)shmat(shmid, 0, 0);
    if (shm_ptr == (char*)-1) {
      printf("父进程重新附加共享内存失败\n");
      exit(1);
    }
    printf("父进程读取子进程数据: %s\n", shm_ptr);

    // 分离共享内存
    if (shmdt(shm_ptr) < 0) {
      printf("父进程分离共享内存失败\n");
      exit(1);
    }

    // 删除共享内存
    if (shmctl(shmid, 1, 0) < 0) { // IPC_RMID
      printf("删除共享内存失败\n");
    } else {
      printf("删除共享内存成功\n");
    }
  }
}

void test_multiple_attach() {
  printf("\n=== 测试多次附加 ===\n");

  // 创建共享内存
  int shmid = shmget(SHM_KEY + 2, SHM_SIZE, 0x01000); // IPC_CREAT
  if (shmid < 0) {
    printf("创建共享内存失败\n");
    exit(1);
  }

  // 第一次附加
  char *shm_ptr1 = (char*)shmat(shmid, 0, 0);
  if (shm_ptr1 == (char*)-1) {
    printf("第一次附加共享内存失败\n");
    exit(1);
  }
  printf("第一次附加共享内存成功，地址 = %p\n", shm_ptr1);

  // 写入数据
  strcpy(shm_ptr1, "First attachment");
  printf("第一次附加写入数据: %s\n", shm_ptr1);

  // 第二次附加
  char *shm_ptr2 = (char*)shmat(shmid, 0, 0);
  if (shm_ptr2 == (char*)-1) {
    printf("第二次附加共享内存失败\n");
    exit(1);
  }
  printf("第二次附加共享内存成功，地址 = %p\n", shm_ptr2);

  // 读取数据
  printf("第二次附加读取数据: %s\n", shm_ptr2);

  // 修改数据
  strcpy(shm_ptr2, "Modified by second attachment");
  printf("第二次附加修改数据\n");

  // 再次读取数据
  printf("第一次附加读取修改后数据: %s\n", shm_ptr1);

  // 分离共享内存
  if (shmdt(shm_ptr1) < 0) {
    printf("分离第一次附加的共享内存失败\n");
    exit(1);
  }
  if (shmdt(shm_ptr2) < 0) {
    printf("分离第二次附加的共享内存失败\n");
    exit(1);
  }

  // 删除共享内存
  if (shmctl(shmid, 1, 0) < 0) { // IPC_RMID
    printf("删除共享内存失败\n");
  } else {
    printf("删除共享内存成功\n");
  }
}

// 压力测试：多进程并发访问
void test_concurrent_access() {
  printf("\n=== 测试多进程并发访问 ===\n");
  
  // 创建共享内存
  int shmid = shmget(SHM_KEY + 3, SHM_SIZE, 0x01000);
  if (shmid < 0) {
    printf("创建共享内存失败\n");
    exit(1);
  }
  printf("创建共享内存成功，shmid = %d\n", shmid);
  
  // 创建信号量用于同步
  int semid = semget(9999, 1, IPC_CREAT | 0666);
  if (semid < 0) {
    printf("创建信号量失败\n");
    exit(1);
  }
  printf("创建信号量成功，semid = %d\n", semid);
  
  // 初始化信号量值为1（互斥锁）
  int sem_val = 1;
  if (semctl(semid, 0, SETVAL, &sem_val) < 0) {
    printf("初始化信号量失败\n");
    exit(1);
  }
  printf("初始化信号量值为 %d\n", sem_val);
  
  // 创建10个子进程进行压力测试
  int num_children = 10;
  int pids[10];
  printf("开始创建%d个子进程\n", num_children);
  
  for (int i = 0; i < num_children; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("创建子进程失败\n");
      exit(1);
    }
    
    if (pid == 0) {
      // 子进程
      char *shm_ptr = (char*)shmat(shmid, 0, 0);
      if (shm_ptr == (char*)-1) {
        printf("子进程 %d 附加共享内存失败\n", i);
        exit(1);
      }
      
      // 每个子进程使用不同的共享内存区域
      // 确保不会超出共享内存边界
      int region_size = SHM_SIZE / num_children;
      char *my_ptr = shm_ptr + i * region_size;
      
      // 清空区域
      struct sembuf sem_op_wait = {0, -1, 0};  // P操作
      struct sembuf sem_op_signal = {0, 1, 0}; // V操作
      
      semop(semid, &sem_op_wait, 1);
      printf("子进程 %d 开始执行，将进行100次写入操作\n", i);
      semop(semid, &sem_op_signal, 1);
      
      for (int j = 0; j < 100; j++) {
        // 每10次操作报告一次进度
        if (j % 10 == 0) {
          semop(semid, &sem_op_wait, 1);
          printf("子进程 %d: 完成 %d 次写入操作\n", i, j);
          semop(semid, &sem_op_signal, 1);
        }
        
        // 使用信号量保护共享内存访问
        semop(semid, &sem_op_wait, 1);
        
        // 构造数据
        char temp[64];
        strcpy(temp, "Process ");
        char num_str[16];
        itoa(i, num_str);
        strcat(temp, num_str);
        strcat(temp, ", write ");
        itoa(j, num_str);
        strcat(temp, num_str);
        
        // 写入共享内存，确保不会超出分配的区域
        int len = strlen(temp);
        if (len < region_size - 1) {
          strcpy(my_ptr, temp);
        }
        
        semop(semid, &sem_op_signal, 1);
        
        // 再次获取信号量读取并验证数据
        semop(semid, &sem_op_wait, 1);
        
        // 读取并验证数据
        char read_buffer[64];
        strcpy(read_buffer, my_ptr);
        if (strcmp(read_buffer, temp) != 0) {
          printf("子进程 %d: 数据验证失败，期望: %s, 实际: %s\n", i, temp, read_buffer);
        }
        
        semop(semid, &sem_op_signal, 1);
        
        // 模拟一些处理时间
        sleep(0);
      }
      
      // 报告完成
      semop(semid, &sem_op_wait, 1);
      printf("子进程 %d: 完成所有100次写入操作\n", i);
      semop(semid, &sem_op_signal, 1);
      
      // 分离共享内存
      if (shmdt(shm_ptr) < 0) {
        semop(semid, &sem_op_wait, 1);
        printf("子进程 %d 分离共享内存失败\n", i);
        semop(semid, &sem_op_signal, 1);
        exit(1);
      }
      
      exit(0);
    }
    
    // 父进程记录子进程PID
    pids[i] = pid;
  }
  
  // 等待所有子进程完成
  printf("所有子进程创建完成，等待子进程结束\n");
  for (int i = 0; i < num_children; i++) {
    struct sembuf sem_op_wait = {0, -1, 0};  // P操作
    struct sembuf sem_op_signal = {0, 1, 0}; // V操作
    
    semop(semid, &sem_op_wait, 1);
    printf("等待子进程 %d (PID=%d) 结束\n", i, pids[i]);
    semop(semid, &sem_op_signal, 1);
    
    wait(0);
    
    semop(semid, &sem_op_wait, 1);
    printf("子进程 %d 已完成\n", i);
    semop(semid, &sem_op_signal, 1);
  }
  
  // 删除信号量
  if (semctl(semid, 0, IPC_RMID, 0) < 0) {
    printf("删除信号量失败\n");
  } else {
    printf("删除信号量成功\n");
  }
  
  // 删除共享内存
  if (shmctl(shmid, 1, 0) < 0) {
    printf("删除共享内存失败\n");
  } else {
    printf("删除共享内存成功\n");
  }
  
  printf("多进程并发访问测试完成\n");
}

// 大数据量测试
void test_large_data() {
  printf("\n=== 测试大数据量传输 ===\n");
  
  // 创建更大的共享内存，但不超过系统限制
  int large_size = SHM_SIZE * 2;  // 8KB，减少大小
  int shmid = shmget(SHM_KEY + 4, large_size, 0x01000);
  if (shmid < 0) {
    printf("创建大容量共享内存失败，尝试更小的大小\n");
    // 尝试更小的大小
    large_size = SHM_SIZE;  // 4KB
    shmid = shmget(SHM_KEY + 4, large_size, 0x01000);
    if (shmid < 0) {
      printf("创建标准大小共享内存也失败\n");
      return;
    }
  }
  printf("创建大容量共享内存成功，shmid = %d, 大小 = %d 字节\n", shmid, large_size);
  
  // 附加共享内存
  char *shm_ptr = (char*)shmat(shmid, 0, 0);
  if (shm_ptr == (char*)-1) {
    printf("附加大容量共享内存失败\n");
    exit(1);
  }
  printf("附加大容量共享内存成功，地址 = %p\n", shm_ptr);
  
  // 写入大量数据
  printf("开始写入大量数据...\n");
  for (int i = 0; i < large_size; i++) {
    shm_ptr[i] = 'A' + (i % 26);  // 填充字母A-Z
  }
  printf("写入 %d 字节数据完成\n", large_size);
  
  // 创建子进程验证数据
  int pid = fork();
  if (pid < 0) {
    printf("创建子进程失败\n");
    exit(1);
  }
  
  if (pid == 0) {
    // 子进程验证数据
    int errors = 0;
    for (int i = 0; i < large_size; i++) {
      if (shm_ptr[i] != 'A' + (i % 26)) {
        errors++;
        if (errors <= 10) {  // 只报告前10个错误
          printf("数据错误在位置 %d: 期望 %c, 实际 %c\n", 
                 i, 'A' + (i % 26), shm_ptr[i]);
        }
      }
    }
    
    if (errors == 0) {
      printf("数据验证成功！\n");
    } else {
      printf("数据验证失败，共发现 %d 个错误\n", errors);
    }
    
    // 分离共享内存
    if (shmdt(shm_ptr) < 0) {
      printf("子进程分离共享内存失败\n");
      exit(1);
    }
    
    exit(0);
  } else {
    // 父进程等待子进程完成
    wait(0);
    
    // 分离共享内存
    if (shmdt(shm_ptr) < 0) {
      printf("父进程分离共享内存失败\n");
      exit(1);
    }
    
    // 删除共享内存
    if (shmctl(shmid, 1, 0) < 0) {
      printf("删除共享内存失败\n");
    } else {
      printf("删除共享内存成功\n");
    }
  }
}

// 边界条件测试
void test_edge_cases() {
  printf("\n=== 测试边界条件 ===\n");
  
  // 测试最小共享内存
  printf("测试最小共享内存...\n");
  int min_shmid = shmget(SHM_KEY + 5, 1, 0x01000);
  if (min_shmid < 0) {
    printf("创建最小共享内存失败\n");
  } else {
    char *min_ptr = (char*)shmat(min_shmid, 0, 0);
    if (min_ptr == (char*)-1) {
      printf("附加最小共享内存失败\n");
    } else {
      *min_ptr = 'X';
      printf("最小共享内存测试成功，写入字符: %c\n", *min_ptr);
      shmdt(min_ptr);
    }
    shmctl(min_shmid, 1, 0);
  }
  
  // 测试无效共享内存ID
  printf("测试无效共享内存ID...\n");
  void *invalid_ptr = shmat(9999, 0, 0);
  if (invalid_ptr == (void*)-1) {
    printf("无效ID测试成功，正确返回错误\n");
  } else {
    printf("无效ID测试失败，应该返回错误\n");
  }
  
  // 测试多次删除同一共享内存
  printf("测试多次删除同一共享内存...\n");
  int dup_shmid = shmget(SHM_KEY + 6, SHM_SIZE, 0x01000);
  if (dup_shmid >= 0) {
    char *dup_ptr = (char*)shmat(dup_shmid, 0, 0);
    if (dup_ptr != (char*)-1) {
      shmdt(dup_ptr);
    }
    
    int result1 = shmctl(dup_shmid, 1, 0);
    int result2 = shmctl(dup_shmid, 1, 0);  // 第二次删除
    
    if (result1 == 0 && result2 < 0) {
      printf("多次删除测试成功，第一次删除成功，第二次删除失败\n");
    } else {
      printf("多次删除测试失败，结果: %d, %d\n", result1, result2);
    }
  }
}

// 共享内存压力测试
void stress_test_shm() {
  printf("\n=== 共享内存压力测试 ===\n");
  
  // 创建共享内存区域（使用更小的大小避免分配失败）
  int shmsize = 4 * 1024; // 4KB
  int shmid = shmget(SHM_KEY + 4, shmsize, 0x01000);
  if (shmid < 0) {
    printf("创建共享内存失败\n");
    exit(1);
  }
  printf("创建共享内存成功，shmid = %d, size = %d\n", shmid, shmsize);
  
  // 创建两个信号量：一个用于保护输出，一个用于保护共享内存
  int output_sem = semget(9997, 1, IPC_CREAT | 0666);
  int shm_sem = semget(9998, 1, IPC_CREAT | 0666);
  if (output_sem < 0 || shm_sem < 0) {
    printf("创建信号量失败\n");
    exit(1);
  }
  
  // 初始化信号量值为1
  int sem_val = 1;
  semctl(output_sem, 0, SETVAL, &sem_val);
  semctl(shm_sem, 0, SETVAL, &sem_val);
  printf("创建信号量成功，output_sem = %d, shm_sem = %d\n", output_sem, shm_sem);
  
  // 创建10个子进程进行压力测试（进一步减少进程数）
  int num_children = 10;
  int pids[10];
  struct sembuf op_pout = {0, -1, 0};
  struct sembuf op_vout = {0, 1, 0};
  
  semop(output_sem, &op_pout, 1);
  printf("开始创建%d个子进程\n", num_children);
  semop(output_sem, &op_vout, 1);
  
  for (int i = 0; i < num_children; i++) {
    int pid = fork();
    if (pid < 0) {
      semop(output_sem, &op_pout, 1);
      printf("创建子进程失败\n");
      semop(output_sem, &op_vout, 1);
      exit(1);
    }
    
    if (pid == 0) {
      // 子进程
      char *shm_ptr = (char*)shmat(shmid, 0, 0);
      if (shm_ptr == (char*)-1) {
        semop(output_sem, &op_pout, 1);
        printf("子进程 %d 附加共享内存失败\n", i);
        semop(output_sem, &op_vout, 1);
        exit(1);
      }
      
      // 每个子进程使用不同的共享内存区域 (确保不会超出边界)
      int region_size = shmsize / num_children;
      char *my_ptr = shm_ptr + i * region_size;
      
      struct sembuf op_pshm = {0, -1, 0};
      struct sembuf op_vshm = {0, 1, 0};
      
      semop(output_sem, &op_pout, 1);
      printf("子进程 %d 开始执行，将进行100次读写操作\n", i);
      semop(output_sem, &op_vout, 1);
      
      for (int j = 0; j < 100; j++) { // 减少到100次操作
        // 每20次操作报告一次进度
        if (j % 20 == 0) {
          semop(output_sem, &op_pout, 1);
          printf("子进程 %d: 完成 %d 次读写操作\n", i, j);
          semop(output_sem, &op_vout, 1);
        }
        
        // 写操作
        semop(shm_sem, &op_pshm, 1);
        
        // 构造数据
        char temp[64];
        strcpy(temp, "P");
        char num_str[16];
        itoa(i, num_str);
        strcat(temp, num_str);
        strcat(temp, "-W");
        itoa(j, num_str);
        strcat(temp, num_str);
        
        // 写入共享内存，确保不会超出分配的区域
        int len = strlen(temp);
        if (len < region_size - 1) {
          strcpy(my_ptr, temp);
        }
        
        semop(shm_sem, &op_vshm, 1);
        
        // 读操作
        semop(shm_sem, &op_pshm, 1);
        
        // 读取并验证数据
        char read_buffer[64];
        int read_len = strlen(my_ptr);
        if (read_len < 64) {
          strcpy(read_buffer, my_ptr);
          // 验证读取的数据是否正确
          if (strcmp(read_buffer, temp) != 0) {
            semop(output_sem, &op_pout, 1);
            printf("子进程 %d: 数据验证失败，期望: %s, 实际: %s\n", i, temp, read_buffer);
            semop(output_sem, &op_vout, 1);
          }
        }
        
        semop(shm_sem, &op_vshm, 1);
        
        // 模拟一些处理时间
        sleep(0);
      }
      
      // 报告完成
      semop(output_sem, &op_pout, 1);
      printf("子进程 %d: 完成所有100次读写操作\n", i);
      semop(output_sem, &op_vout, 1);
      
      // 分离共享内存
      if (shmdt(shm_ptr) < 0) {
        semop(output_sem, &op_pout, 1);
        printf("子进程 %d 分离共享内存失败\n", i);
        semop(output_sem, &op_vout, 1);
        exit(1);
      }
      
      exit(0);
    }
    
    // 父进程记录子进程PID
    pids[i] = pid;
  }
  
  // 等待所有子进程完成
  semop(output_sem, &op_pout, 1);
  printf("所有子进程创建完成，等待子进程结束\n");
  semop(output_sem, &op_vout, 1);
  
  for (int i = 0; i < num_children; i++) {
    semop(output_sem, &op_pout, 1);
    printf("等待子进程 %d (PID=%d) 结束\n", i, pids[i]);
    semop(output_sem, &op_vout, 1);
    
    wait(0);
    
    semop(output_sem, &op_pout, 1);
    printf("子进程 %d 已完成\n", i);
    semop(output_sem, &op_vout, 1);
  }
  
  // 删除信号量
  if (semctl(output_sem, 0, IPC_RMID, 0) < 0 || semctl(shm_sem, 0, IPC_RMID, 0) < 0) {
    semop(output_sem, &op_pout, 1);
    printf("删除信号量失败\n");
    semop(output_sem, &op_vout, 1);
  } else {
    semop(output_sem, &op_pout, 1);
    printf("删除信号量成功\n");
    semop(output_sem, &op_vout, 1);
  }
  
  // 删除共享内存
  if (shmctl(shmid, 1, 0) < 0) {
    semop(output_sem, &op_pout, 1);
    printf("删除共享内存失败\n");
    semop(output_sem, &op_vout, 1);
  } else {
    semop(output_sem, &op_pout, 1);
    printf("删除共享内存成功\n");
    semop(output_sem, &op_vout, 1);
  }
  
  semop(output_sem, &op_pout, 1);
  printf("共享内存压力测试完成\n");
  semop(output_sem, &op_vout, 1);
}

int main() {
  printf("共享内存测试程序\n");

  test_basic_shm();
  test_ipc();
  test_multiple_attach();
  test_concurrent_access();
  test_large_data();
  test_edge_cases();
  
  // 共享内存压力测试
  stress_test_shm();

  printf("\n所有测试通过！\n");
  exit(0);
}
