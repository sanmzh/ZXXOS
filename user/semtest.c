/*
 * 信号量测试程序
 * 
 * 本程序测试基于System V IPC风格的信号量实现，包括:
 * 1. semget() - 创建或获取信号量集
 * 2. semop()  - 信号量操作(P/V操作)
 * 3. semctl() - 信号量控制(设置值、获取值、删除等)
 *
 * 测试内容涵盖:
 * - 基本信号量功能测试
 * - 进程间同步测试
 * - 多信号量操作测试
 * - 并发压力测试(死锁预防)
 */

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/ipc.h"

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
        printf("删除信号量集失败4\n");
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
        printf("子进程: 任务完成\n");

        exit(0);
    } else {
        // 父进程
        // 不需要sleep，直接释放信号量
        // 此时子进程可能还没开始，也可能已经在等待，
        // 但由于信号量初始值为0，父进程先释放也不会有问题
        
        printf("父进程: 释放信号量...\n");
        
        // V操作（释放信号量）
        struct sembuf op_v = {0, 1, 0};
        if (semop(semid, &op_v, 1) < 0) {
            printf("父进程: V操作失败\n");
            return;
        }

        // 等待子进程结束
        wait(0);
        
        // 删除信号量集
        if (semctl(semid, 0, IPC_RMID, 0) < 0) {
            printf("删除信号量集失败3\n");
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
    
    // 检查信号量数量是否符合预期
    if (info.sem_nsems != 3) {
        printf("错误: 预期3个信号量，实际为%d个\n", info.sem_nsems);
        return;
    }

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
        printf("删除信号量集失败2\n");
        return;
    }
    printf("删除信号量集成功\n");

    printf("=== 多信号量测试完成 ===\n\n");
}

// 压力测试：大量并发进程测试信号量
void stress_test() {
    printf("=== 信号量压力测试 ===\n");
    
    // 创建两个信号量集用于测试死锁预防
    int semid1 = semget(9001, 1, IPC_CREAT | 0666);
    int semid2 = semget(9002, 1, IPC_CREAT | 0666);
    if (semid1 < 0 || semid2 < 0) {
        printf("创建信号量集失败\n");
        return;
    }
    
    // 初始化信号量值为1（互斥锁）
    int value = 1;
    if (semctl(semid1, 0, SETVAL, &value) < 0 || semctl(semid2, 0, SETVAL, &value) < 0) {
        printf("初始化信号量失败\n");
        return;
    }
    
    // 创建专门用于保护输出的信号量
    int output_sem = semget(9003, 1, IPC_CREAT | 0666);
    if (output_sem < 0) {
        printf("创建输出保护信号量失败\n");
        return;
    }
    if (semctl(output_sem, 0, SETVAL, &value) < 0) {
        printf("初始化输出保护信号量失败\n");
        return;
    }
    
    printf("创建信号量集成功: semid1=%d, semid2=%d, output_sem=%d\n", semid1, semid2, output_sem);
    
    // 增加子进程数量到10个进行并发测试
    int num_processes = 10;
    int pids[10];
    printf("开始创建%d个子进程\n", num_processes);
    
    for (int i = 0; i < num_processes; i++) {
        int pid = fork();
        if (pid < 0) {
            printf("创建子进程%d失败\n", i);
            return;
        }
        
        if (pid == 0) {
            // 子进程执行信号量操作
            struct sembuf op_p1 = {0, -1, 0};
            struct sembuf op_v1 = {0, 1, 0};
            struct sembuf op_p2 = {0, -1, 0};
            struct sembuf op_v2 = {0, 1, 0};
            struct sembuf op_pout = {0, -1, 0};
            struct sembuf op_vout = {0, 1, 0};
            
            // 使用专门的输出信号量保护输出
            semop(output_sem, &op_pout, 1);
            printf("子进程%d开始执行，将进行500次信号量操作\n", i);
            semop(output_sem, &op_vout, 1);
            
            for (int j = 0; j < 500; j++) {
                // 每50次操作报告一次进度
                if (j % 50 == 0) {
                    semop(output_sem, &op_pout, 1);
                    printf("子进程%d: 完成%d次操作\n", i, j);
                    semop(output_sem, &op_vout, 1);
                }
                
                // 所有进程都按相同顺序获取信号量，避免死锁
                // 先获取semid1
                if (semop(semid1, &op_p1, 1) < 0) {
                    semop(output_sem, &op_pout, 1);
                    printf("子进程%d: 请求semid1失败\n", i);
                    semop(output_sem, &op_vout, 1);
                    exit(1);
                }
                
                // 再获取semid2
                if (semop(semid2, &op_p2, 1) < 0) {
                    // 如果获取semid2失败，先释放semid1
                    semop(semid1, &op_v1, 1);
                    semop(output_sem, &op_pout, 1);
                    printf("子进程%d: 请求semid2失败\n", i);
                    semop(output_sem, &op_vout, 1);
                    exit(1);
                }
                
                // 模拟一些工作
                sleep(0);  // 让出CPU时间片
                
                // 释放信号量，按相反顺序
                semop(semid2, &op_v2, 1);
                semop(semid1, &op_v1, 1);
            }
            
            // 报告完成
            semop(output_sem, &op_pout, 1);
            printf("子进程%d: 完成所有500次操作\n", i);
            semop(output_sem, &op_vout, 1);
            
            exit(0);
        }
        
        // 父进程记录子进程PID
        pids[i] = pid;
    }
    
    printf("所有子进程创建完成，等待子进程结束\n");
    
    // 等待所有子进程完成
    for (int i = 0; i < num_processes; i++) {
        struct sembuf op_pout = {0, -1, 0};
        struct sembuf op_vout = {0, 1, 0};
        semop(output_sem, &op_pout, 1);  // 使用输出保护信号量
        printf("等待子进程%d (PID=%d)结束\n", i, pids[i]);
        semop(output_sem, &op_vout, 1);
        wait(0);
        semop(output_sem, &op_pout, 1);
        printf("子进程%d已完成\n", i);
        semop(output_sem, &op_vout, 1);
    }
    
    printf("所有子进程已完成\n");
    
    // 清理资源
    if (semctl(semid1, 0, IPC_RMID, 0) < 0 || semctl(semid2, 0, IPC_RMID, 0) < 0 || semctl(output_sem, 0, IPC_RMID, 0) < 0) {
        printf("删除信号量集失败1\n");
        return;
    }
    
    printf("信号量压力测试完成\n");
}

int main() {
    printf("信号量测试开始\n\n");

    // 测试基本功能
    test_basic_sem();
    
    // 测试进程同步
    test_sem_sync();
    
    // 测试多个信号量
    test_multiple_sems();
    
    // 压力测试
    stress_test();
    
    printf("信号量测试完成\n");
    exit(0);
}
