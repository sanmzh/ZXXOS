#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 模拟任务函数，执行一些计算并打印信息
void task(int id) {
    // 执行一些计算工作
    int sum = 0;
    for (int i = 0; i < 1000000; i++) {
        sum += i;
    }
}

// 长时间运行的任务，用于测试时间片切换
void long_task(int id) {
    
    // 执行更长时间的计算，确保会发生多次时间片切换
    volatile long sum = 0;
    for (long i = 0; i < 10000000; i++) {
        sum += i;
    }
    
}

// 测试基本RR调度功能
void test_basic_rr() {
    printf("\n=== Testing Basic RR Scheduler ===\n");
    
    int pid1, pid2, pid3; 
    if((pid1=fork())==0) { 
        set_timeslice(1); 
        task(1); 
        exit(0); 
    } 
    if((pid2=fork())==0) { 
        set_timeslice(2); 
        task(2); 
        exit(0); 
    } 
    if((pid3=fork())==0) { 
        set_timeslice(3); 
        task(3); 
        exit(0); 
    } 
    wait(0); 
    wait(0); 
    wait(0); 
    
    printf("Basic RR Test Completed\n");
}

// 测试长时间运行任务的时间片切换
void test_long_running_tasks() {
    printf("\n=== Testing Long Running Tasks ===\n");
    
    int pid1, pid2;
    if((pid1=fork())==0) { 
        set_timeslice(2); 
        long_task(1); 
        exit(0); 
    } 
    if((pid2=fork())==0) { 
        set_timeslice(3); 
        long_task(2); 
        exit(0); 
    } 
    wait(0); 
    wait(0); 
    
    printf("Long Running Tasks Test Completed\n");
}

// 测试不同时间片设置的影响
void test_different_timeslices() {
    printf("\n=== Testing Different Timeslice Settings ===\n");
    
    int pid1, pid2, pid3;
    if((pid1=fork())==0) { 
        set_timeslice(1);  // 最小时间片
        for(int i = 0; i < 5; i++) {
            // 短暂计算
            volatile int sum = 0;
            for(int j = 0; j < 100000; j++) sum += j;
        }
        exit(0); 
    } 
    if((pid2=fork())==0) { 
        set_timeslice(5);  // 中等时间片
        for(int i = 0; i < 5; i++) {
            // 相同的计算量
            volatile int sum = 0;
            for(int j = 0; j < 100000; j++) sum += j;
        }
        exit(0); 
    } 
    if((pid3=fork())==0) { 
        set_timeslice(10);  // 大时间片
        for(int i = 0; i < 5; i++) {
            // 相同的计算量
            volatile int sum = 0;
            for(int j = 0; j < 100000; j++) sum += j;
        }
        exit(0); 
    } 
    wait(0); 
    wait(0); 
    wait(0); 
    
    printf("Different Timeslices Test Completed\n");
}

// 测试错误参数处理
void test_error_handling() {
    printf("\n=== Testing Error Handling ===\n");
    
    int pid;
    if((pid=fork())==0) { 
        int result = set_timeslice(0);  // 无效参数
        printf("set_timeslice(0) returned: %d (expected: -1)\n", result);
        
        result = set_timeslice(-1);  // 无效参数
        printf("set_timeslice(-1) returned: %d (expected: -1)\n", result);
        
        result = set_timeslice(5);  // 有效参数
        printf("set_timeslice(5) returned: %d (expected: 0)\n", result);
        
        exit(0); 
    } 
    wait(0);
    
    printf("Error Handling Test Completed\n");
}

// 测试多级fork场景
void test_nested_forks() {
    printf("\n=== Testing Nested Forks ===\n");
    
    int pid1, pid2;
    if((pid1=fork())==0) { 
        set_timeslice(2); 
        printf("Parent process started\n");
        
        if((pid2=fork())==0) { 
            set_timeslice(3); 
            printf("Child process started\n");
            task(1);
            exit(0);
        }
        
        task(2);
        wait(0);
        printf("Parent process completed\n");
        exit(0); 
    } 
    wait(0);
    
    printf("Nested Forks Test Completed\n");
}

int main() { 
    printf("RR Scheduler Comprehensive Test Suite\n"); 
    
    // 运行各种测试
    test_basic_rr();
    test_long_running_tasks();
    test_different_timeslices();
    test_error_handling();
    test_nested_forks();
    
    printf("\nAll RR Scheduler Tests Completed\n"); 
    exit(0); 
}