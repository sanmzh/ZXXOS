#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

// Linux下的MLFQ测试用例
// 该测试用例创建5种不同类型的工作负载，验证Linux调度器的行为

// CPU密集型任务 - 持续计算，不释放CPU
void cpu_intensive_task(int pid) {
    printf("Linux Process %d (CPU-intensive): started\n", pid);
    
    // 执行大量计算，模拟CPU密集型任务
    volatile long sum = 0;
    for (long i = 0; i < 200000000; i++) {  // 与xv6版本相同的计算量
        sum += i;
    }
    
    printf("Linux Process %d (CPU-intensive): completed\n", pid);
    exit(0);
}

// I/O密集型任务 - 频繁sleep
void io_intensive_task(int pid) {
    printf("Linux Process %d (I/O-intensive): started\n", pid);
    
    // 频繁sleep，模拟I/O密集型任务
    for (int i = 0; i < 30; i++) {  // 与xv6版本相同的sleep次数
        sleep(1);  // 每次sleep 1秒
    }
    
    printf("Linux Process %d (I/O-intensive): completed\n", pid);
    exit(0);
}

// 中等CPU密集型任务 - 适度计算
void moderate_cpu_task(int pid) {
    printf("Linux Process %d (Moderate CPU): started\n", pid);
    
    // 适度计算，模拟中等CPU密集型任务
    volatile long sum = 0;
    for (long i = 0; i < 100000000; i++) {  // 与xv6版本相同的计算量
        sum += i;
    }
    
    printf("Linux Process %d (Moderate CPU): completed\n", pid);
    exit(0);
}

// 混合型任务 - 交替CPU计算和sleep
void mixed_task(int pid) {
    printf("Linux Process %d (Mixed): started\n", pid);
    
    // 混合CPU计算和I/O操作
    for (int i = 0; i < 10; i++) {
        // CPU计算阶段
        volatile long sum = 0;
        for (long j = 0; j < 50000000; j++) {  // 与xv6版本相同的计算量
            sum += j;
        }
        
        // I/O操作阶段
        sleep(2);  // 休眠2秒
    }
    
    printf("Linux Process %d (Mixed): completed\n", pid);
    exit(0);
}

// 长时间休眠任务 - 模拟长时间等待I/O
void long_sleep_task(int pid) {
    printf("Linux Process %d (Long Sleep): started\n", pid);
    
    // 休眠25秒，与xv6版本相同
    sleep(25);
    
    printf("Linux Process %d (Long Sleep): completed\n", pid);
    exit(0);
}

int main() {
    printf("Testing Linux Scheduler - MLFQ-like behavior\n");
    
    // 创建5个子进程，每个执行不同类型的任务
    for (int i = 1; i <= 5; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // 子进程
            switch (i) {
                case 1:
                    cpu_intensive_task(i);
                    break;
                case 2:
                    io_intensive_task(i);
                    break;
                case 3:
                    moderate_cpu_task(i);
                    break;
                case 4:
                    mixed_task(i);
                    break;
                case 5:
                    long_sleep_task(i);
                    break;
                default:
                    exit(0);
            }
        }
    }
    
    // 父进程等待所有子进程完成
    for (int i = 1; i <= 5; i++) {
        int status;
        wait(&status);
    }
    
    printf("Linux Scheduler Test Completed\n");
    return 0;
}