#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// MLFQ测试用例
// 该测试用例创建5种不同类型的工作负载，验证MLFQ调度器的行为

// CPU密集型任务 - 持续计算，不释放CPU
void cpu_intensive_task(int pid) {
    int initial_priority = 5;  // 调整初始优先级为中等，让MLFQ有机会降低它
    set_priority(initial_priority);
    
    // 执行大量计算，模拟CPU密集型任务
    volatile long sum = 0;
    // 分成多个阶段，每个阶段输出当前优先级，观察MLFQ的动态调整
    for (int phase = 0; phase < 8; phase++) {  // 增加阶段数，从5到8
        for (long i = 0; i < 300000000; i++) {  // 增加计算量，每个阶段300M次循环
            sum += i;
        }
        // 输出当前优先级，观察MLFQ的动态调整
        int current_priority = get_priority();
        printf("P1(CPU): phase %d, current priority %d\n", phase+1, current_priority);
    }
    
    int final_priority = get_priority();
    printf("MLFQ Scheduler Process %d (CPU-intensive) with initial priority %d and final priority %d completed\n", 
           pid, initial_priority, final_priority);
    exit(0);
}

// I/O密集型任务 - 频繁sleep
void io_intensive_task(int pid) {
    int initial_priority = 5;  // 调整初始优先级为中等，让MLFQ有机会提升它
    set_priority(initial_priority);
    
    // 频繁sleep，模拟I/O密集型任务
    for (int i = 0; i < 3; i++) {  // 进一步减少sleep次数，从5次到3次
        sleep(1);  // 每次sleep 1秒
        // 输出当前优先级，观察MLFQ的动态调整
        int current_priority = get_priority();
        printf("P2(I/O): iteration %d, current priority %d\n", i+1, current_priority);
    }
    
    int final_priority = get_priority();
    printf("MLFQ Scheduler Process %d (I/O-intensive) with initial priority %d and final priority %d completed\n", 
           pid, initial_priority, final_priority);
    exit(0);
}

// 中等CPU密集型任务 - 适中的计算量
void moderate_cpu_task(int pid) {
    int initial_priority = 5;  // 调整初始优先级为中等
    set_priority(initial_priority);
    
    // 执行中等量计算，模拟中等CPU密集型任务
    volatile long sum = 0;
    // 分成多个阶段，每个阶段输出当前优先级，观察MLFQ的动态调整
    for (int phase = 0; phase < 3; phase++) {
        for (long i = 0; i < 100000000; i++) {  // 适中的计算量，每个阶段100M次循环
            sum += i;
        }
        // 输出当前优先级，观察MLFQ的动态调整
        int current_priority = get_priority();
        printf("P3(Moderate): phase %d, current priority %d\n", phase+1, current_priority);
    }
    
    int final_priority = get_priority();
    printf("MLFQ Scheduler Process %d (Moderate CPU) with initial priority %d and final priority %d completed\n", 
           pid, initial_priority, final_priority);
    exit(0);
}

// 混合型任务 - 交替进行计算和I/O
void mixed_task(int pid) {
    int initial_priority = 5;  // 调整初始优先级为中等
    set_priority(initial_priority);
    
    // 交替进行计算和I/O，模拟混合型任务
    volatile long sum = 0;
    for (int i = 0; i < 4; i++) {  // 减少循环次数，从5次到4次
        // 计算阶段 - 减少计算量，使其I/O特征更明显
        for (long j = 0; j < 30000000; j++) {  // 减少计算量，每个循环30M次
            sum += j;
        }
        // I/O阶段 - 增加sleep时间，使I/O特征更明显
        sleep(3);  // 增加sleep时间，从2秒到3秒
        // 输出当前优先级，观察MLFQ的动态调整
        int current_priority = get_priority();
        printf("P4(Mixed): iteration %d, current priority %d\n", i+1, current_priority);
    }
    
    int final_priority = get_priority();
    printf("MLFQ Scheduler Process %d (Mixed) with initial priority %d and final priority %d completed\n", 
           pid, initial_priority, final_priority);
    exit(0);
}

// 长时间休眠任务 - 模拟长时间等待I/O，初始优先级为3
void long_sleep_task(int pid) {
    int initial_priority = 5;  // 调整初始优先级为中等
    set_priority(initial_priority);
    
    // 休眠25秒，确保在其他进程完成后完成
    sleep(25);
    // 输出当前优先级，观察MLFQ的动态调整
    int current_priority = get_priority();
    printf("P5(Long Sleep): after sleep, current priority %d\n", current_priority);
    
    int final_priority = get_priority();
    printf("MLFQ Scheduler Process %d (Long Sleep) with initial priority %d and final priority %d completed\n", 
           pid, initial_priority, final_priority);
    exit(0);
}

int main() {
    printf("Testing MLFQ Scheduler - Basic\n");
    
    // 创建5个子进程，每个执行不同类型的任务
    for (int i = 1; i <= 5; i++) {
        int pid = fork();
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
        wait(0);
    }
    
    printf("MLFQ with Priorities Test Completed\n");
    exit(0);
}