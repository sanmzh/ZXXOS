
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/sem.h"

// 测试信号量的基本功能
void test_basic_sem() {
    printf("=== 测试信号量基本功能 ===\n");

    // 创建一个信号量集，包含1个信号量
    int semid = semget(1234, 1, IPC_CREAT | 0666);
    if (semid < 0) {
        printf("创建信号量集失败\n");
        return;
    }
    printf("创建信号量集成功，semid=%d\n", semid);

    // 设置信号量初始值为1
    int value = 1;
    if (semctl(semid, 0, SETVAL, &value) < 0) {
        printf("设置信号量值失败\n");
        return;
    }
    printf("设置信号量初始值为1\n");

    // 获取信号量值
    int cur_value = semctl(semid, 0, GETVAL, 0);
    printf("当前信号量值: %d\n", cur_value);

    // P操作（等待信号量）
    struct sembuf op_p = {0, -1, 0}; // 对第0个信号量执行P操作
    if (semop(semid, &op_p, 1) < 0) {
        printf("P操作失败\n");
        return;
    }
    printf("执行P操作后，信号量值: %d\n", semctl(semid, 0, GETVAL, 0));

    // V操作（释放信号量）
    struct sembuf op_v = {0, 1, 0}; // 对第0个信号量执行V操作
    if (semop(semid, &op_v, 1) < 0) {
        printf("V操作失败\n");
        return;
    }
    printf("执行V操作后，信号量值: %d\n", semctl(semid, 0, GETVAL, 0));

    // 删除信号量集
    if (semctl(semid, 0, IPC_RMID, 0) < 0) {
        printf("删除信号量集失败\n");
        return;
    }
    printf("删除信号量集成功\n");

    printf("=== 基本功能测试完成 ===\n\n");
}

// 测试信号量的进程同步功能
void test_sem_sync() {
    printf("=== 测试信号量进程同步功能 ===\n");

    // 创建一个信号量集，包含1个信号量
    int semid = semget(5678, 1, IPC_CREAT | 0666);
    if (semid < 0) {
        printf("创建信号量集失败\n");
        return;
    }
    printf("创建信号量集成功，semid=%d\n", semid);

    // 设置信号量初始值为0
    int value = 0;
    if (semctl(semid, 0, SETVAL, &value) < 0) {
        printf("设置信号量值失败\n");
        return;
    }
    printf("设置信号量初始值为0\n");

    // 创建子进程
    int pid = fork();
    if (pid < 0) {
        printf("创建子进程失败\n");
        return;
    }

    if (pid == 0) {
        // 子进程
        printf("子进程: 等待信号量...\n");

        // P操作（等待信号量）
        struct sembuf op_p = {0, -1, 0};
        if (semop(semid, &op_p, 1) < 0) {
            printf("子进程: P操作失败\n");
            exit(1);
        }

        printf("子进程: 获得信号量，执行任务...\n");
        sleep(10); // 模拟任务执行
        printf("子进程: 任务完成\n");

        exit(0);
    } else {
        // 父进程
        printf("父进程: 等待2秒后释放信号量...\n");
        sleep(20); // 等待2秒

        // V操作（释放信号量）
        struct sembuf op_v = {0, 1, 0};
        if (semop(semid, &op_v, 1) < 0) {
            printf("父进程: V操作失败\n");
            return;
        }

        printf("父进程: 释放信号量\n");

        // 等待子进程结束
        wait(0);

        // 删除信号量集
        if (semctl(semid, 0, IPC_RMID, 0) < 0) {
            printf("删除信号量集失败\n");
            return;
        }
        printf("删除信号量集成功\n");
    }

    printf("=== 进程同步测试完成 ===\n\n");
}

// 测试多个信号量
void test_multiple_sems() {
    printf("=== 测试多个信号量 ===\n");

    // 创建一个信号量集，包含3个信号量
    int semid = semget(9012, 3, IPC_CREAT | 0666);
    if (semid < 0) {
        printf("创建信号量集失败\n");
        return;
    }
    printf("创建信号量集成功，semid=%d\n", semid);

    // 设置信号量初始值
    int values[] = {2, 0, 1};
    for (int i = 0; i < 3; i++) {
        if (semctl(semid, i, SETVAL, &values[i]) < 0) {
            printf("设置信号量%d值失败\n", i);
            return;
        }
    }

    // 获取所有信号量值
    struct semid_ds info;
    if (semctl(semid, 0, IPC_STAT, &info) < 0) {
        printf("获取信号量集信息失败\n");
        return;
    }
    printf("信号量集包含%d个信号量\n", info.sem_nsems);

    for (int i = 0; i < 3; i++) {
        int value = semctl(semid, i, GETVAL, 0);
        printf("信号量%d的值: %d\n", i, value);
    }

    // 对多个信号量进行操作
    struct sembuf ops[] = {
        {0, -1, 0}, // 对第0个信号量执行P操作
        {1, 1, 0},  // 对第1个信号量执行V操作
        {2, -1, 0}  // 对第2个信号量执行P操作
    };

    if (semop(semid, ops, 3) < 0) {
        printf("多信号量操作失败\n");
        return;
    }

    printf("执行多信号量操作后:\n");
    for (int i = 0; i < 3; i++) {
        int value = semctl(semid, i, GETVAL, 0);
        printf("信号量%d的值: %d\n", i, value);
    }

    // 删除信号量集
    if (semctl(semid, 0, IPC_RMID, 0) < 0) {
        printf("删除信号量集失败\n");
        return;
    }
    printf("删除信号量集成功\n");

    printf("=== 多信号量测试完成 ===\n\n");
}

int main() {
    printf("信号量测试开始\n\n");

    test_basic_sem();
    test_sem_sync();
    test_multiple_sems();

    printf("信号量测试完成\n");
    exit(0);
}
