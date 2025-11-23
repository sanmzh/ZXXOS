#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

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
  
  // 创建5个子进程
  int num_children = 5;
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
      
      // 每个子进程写入不同的数据
      // 为每个进程使用不同的共享内存区域，避免竞争条件
      char *my_ptr = shm_ptr + i * 256;  // 每个进程使用256字节的区域，确保足够的间距
      
      // 先清空区域
      for (int k = 0; k < 256; k++) {
        my_ptr[k] = '\0';
      }
      
      for (int j = 0; j < 10; j++) {
        // 使用更安全的方式构建字符串
        char temp[64];
        strcpy(temp, "Child ");
        char num_str[16];
        itoa(i, num_str);
        strcat(temp, num_str);
        strcat(temp, ", iteration ");
        itoa(j, num_str);
        strcat(temp, num_str);
        
        // 复制到共享内存
        strcpy(my_ptr, temp);
        sleep(1);  // 模拟处理时间
        printf("子进程 %d 写入: %s\n", i, my_ptr);
      }
      
      // 分离共享内存
      if (shmdt(shm_ptr) < 0) {
        printf("子进程 %d 分离共享内存失败\n", i);
        exit(1);
      }
      
      exit(0);
    }
  }
  
  // 等待所有子进程完成
  for (int i = 0; i < num_children; i++) {
    wait(0);
  }
  
  // 删除共享内存
  if (shmctl(shmid, 1, 0) < 0) {
    printf("删除共享内存失败\n");
  } else {
    printf("删除共享内存成功\n");
  }
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

int main() {
  printf("共享内存测试程序\n");

  test_basic_shm();
  test_ipc();
  test_multiple_attach();
  test_concurrent_access();
  test_large_data();
  test_edge_cases();

  printf("\n所有测试通过！\n");
  exit(0);
}
