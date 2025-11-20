#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

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

    // 等待子进程
    wait(0);

    // 读取子进程写入的数据
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

int main() {
  printf("共享内存测试程序\n");

  test_basic_shm();
  test_ipc();
  test_multiple_attach();

  printf("\n所有测试通过！\n");
  exit(0);
}
