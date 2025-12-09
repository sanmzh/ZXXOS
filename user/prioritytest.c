#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// CPU密集型计算函数
void cpu_intensive_task(int iterations) {
    volatile int sum = 0;
    for (int i = 0; i < iterations; i++) {
        sum += i;
    }
}

// I/O密集型任务（模拟）
void io_intensive_task(int iterations) {
    for (int i = 0; i < iterations; i++) {
        // 模拟I/O操作，通过sleep让出CPU
        printf("I/O操作 %d/%d\n", i+1, iterations);
        sleep(1);  // 使用更短的休眠时间，sleep(1)约等于1秒
    }
}

// 测试1：基本优先级调度测试
void test_basic_priority() {
    printf("\n=== 测试1：基本优先级调度测试 ===\n");
    printf("创建5个CPU密集型进程，优先级分别为5、10、15、20、25\n");
    printf("预期完成顺序：P1(5) -> P2(10) -> P3(15) -> P4(20) -> P5(25)\n");
    
    int pids[5];
    int priorities[] = {5, 10, 15, 20, 25};
    
    for (int i = 0; i < 5; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            // 子进程
            set_priority(priorities[i]);
            cpu_intensive_task(50000000);  // 减少计算量，加快测试
            printf("P%d (优先级%d) 完成\n", i+1, priorities[i]);
            exit(0);
        }
    }
    
    // 等待所有子进程完成
    for (int i = 0; i < 5; i++) {
        wait(0);
    }
    
    printf("测试1完成\n");
}

// 测试2：动态优先级变化测试
void test_dynamic_priority() {
    printf("\n=== 测试2：动态优先级变化测试 ===\n");
    printf("创建3个进程，初始优先级相同，运行过程中改变优先级\n");
    
    int pid1 = fork();
    if (pid1 == 0) {
        // 子进程1
        set_priority(20);
        printf("子进程1：初始优先级20\n");
        
        // 运行一段时间后提高优先级
        cpu_intensive_task(20000000);
        set_priority(5);
        printf("子进程1：优先级提高到5\n");
        cpu_intensive_task(20000000);
        printf("子进程1完成\n");
        exit(0);
    }
    
    int pid2 = fork();
    if (pid2 == 0) {
        // 子进程2
        set_priority(20);
        printf("子进程2：初始优先级20\n");
        cpu_intensive_task(40000000);
        printf("子进程2完成\n");
        exit(0);
    }
    
    int pid3 = fork();
    if (pid3 == 0) {
        // 子进程3
        set_priority(20);
        printf("子进程3：初始优先级20\n");
        
        // 运行一段时间后降低优先级
        cpu_intensive_task(20000000);
        set_priority(30);
        printf("子进程3：优先级降低到30\n");
        cpu_intensive_task(20000000);
        printf("子进程3完成\n");
        exit(0);
    }
    
    // 等待所有子进程完成
    wait(0);
    wait(0);
    wait(0);
    
    printf("测试2完成\n");
}

// 测试3：混合I/O和CPU密集型任务测试
void test_mixed_tasks() {
    printf("\n=== 测试3：混合I/O和CPU密集型任务测试 ===\n");
    printf("创建CPU密集型和I/O密集型混合任务，观察调度行为\n");
    
    // CPU密集型任务
    int pid1 = fork();
    if (pid1 == 0) {
        set_priority(10);  // 高优先级
        printf("CPU密集型任务开始，优先级10\n");
        cpu_intensive_task(30000000);
        printf("CPU密集型任务完成\n");
        exit(0);
    }
    
    // I/O密集型任务
    int pid2 = fork();
    if (pid2 == 0) {
        set_priority(15);  // 中等优先级
        printf("I/O密集型任务开始，优先级15\n");
        io_intensive_task(5);
        printf("I/O密集型任务完成\n");
        exit(0);
    }
    
    // 另一个CPU密集型任务
    int pid3 = fork();
    if (pid3 == 0) {
        set_priority(20);  // 低优先级
        printf("CPU密集型任务2开始，优先级20\n");
        cpu_intensive_task(30000000);
        printf("CPU密集型任务2完成\n");
        exit(0);
    }
    
    // 等待所有子进程完成
    wait(0);
    wait(0);
    wait(0);
    
    printf("测试3完成\n");
}

// 测试4：饥饿测试
void test_starvation() {
    printf("\n=== 测试4：饥饿测试 ===\n");
    printf("创建高优先级进程持续运行，观察低优先级进程是否会饥饿\n");
    
    // 高优先级进程，持续运行
    for (int i = 0; i < 3; i++) {
        int pid = fork();
        if (pid == 0) {
            set_priority(5);  // 高优先级
            printf("高优先级进程%d开始\n", i+1);
            cpu_intensive_task(20000000);
            printf("高优先级进程%d完成\n", i+1);
            exit(0);
        }
    }
    
    // 低优先级进程
    int low_pid = fork();
    if (low_pid == 0) {
        set_priority(30);  // 低优先级
        printf("低优先级进程开始\n");
        cpu_intensive_task(10000000);
        printf("低优先级进程完成\n");
        exit(0);
    }
    
    // 等待所有子进程完成
    for (int i = 0; i < 4; i++) {
        wait(0);
    }
    
    printf("测试4完成\n");
}

// 测试5：大量进程测试
void test_many_processes() {
    printf("\n=== 测试5：大量进程测试 ===\n");
    printf("创建10个不同优先级的进程，观察调度器性能\n");
    
    int pids[10];
    
    for (int i = 0; i < 10; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            // 子进程
            set_priority((i % 5) * 5 + 5);  // 优先级在5-25之间循环
            printf("进程%d开始，优先级%d\n", i+1, (i % 5) * 5 + 5);
            cpu_intensive_task(10000000);
            printf("进程%d完成\n", i+1);
            exit(0);
        }
    }
    
    // 等待所有子进程完成
    for (int i = 0; i < 10; i++) {
        wait(0);
    }
    
    printf("测试5完成\n");
}

int main(int argc, char *argv[]) {
    printf("高级优先级调度器测试程序\n");
    printf("包含5个不同场景的测试\n");
    
    // 运行所有测试
    test_basic_priority();
    test_dynamic_priority();
    test_mixed_tasks();
    test_starvation();
    test_many_processes();
    
    printf("\n所有测试完成\n");
    exit(0);
}