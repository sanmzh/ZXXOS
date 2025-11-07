
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int pid;

    // 只跟踪fork和getpid系统调用，不跟踪write系统调用
    // SYS_fork=1, SYS_getpid=11
    // 所以掩码应该是 (1<<1)|(1<<11) = 2+2048 = 2050
    int mask = (1 << 1) | (1 << 11);  // fork=1, getpid=11

    printf("开始测试trace系统调用\n");
    printf("设置跟踪掩码: %d\n\n", mask);

    // 启用跟踪
    trace(mask);

    // 测试getpid
    printf("测试getpid系统调用\n");
    pid = getpid();
    printf("当前进程ID: %d\n\n", pid);

    // 测试write
    printf("测试write系统调用\n");
    write(1, "Hello, trace!\n", 14);

    // 测试fork
    printf("\n测试fork系统调用\n");
    pid = fork();
    if(pid == 0) {
        // 子进程
        printf("子进程ID: %d\n\n", getpid());
        exit(0);
    } else {
        // 父进程
        wait(0);
        printf("父进程ID: %d\n\n", getpid());
    }

    // 测试一个未被跟踪的系统调用（如uptime）
    printf("测试未被跟踪的系统调用uptime\n\n");
    uptime();

    printf("trace测试完成\n\n");
    exit(0);
}
