#include "types.h"
#include "param.h"
#include "loongarch.h"
#include "time.h"
#include "defs.h"
#include "memlayout.h"

int rtc_print_date(void)
{
    // 1. 读取RTC原始数据
    uint64_t raw_data = rtcread();
    
    // 2. 解析为tm结构体
    struct tm rtc_time;
    localtime_r(raw_data, &rtc_time);
    
    // 3. 输出日期和时间信息
    printf("\n=== RTC日期时间信息 ===\n");
    
    // 完整日期时间
    printf("完整时间: %04d年%02d月%02d日 %02d时%02d分%02d秒\n",
           rtc_time.tm_year + 1900,
           rtc_time.tm_mon + 1,
           rtc_time.tm_mday,
           rtc_time.tm_hour,
           rtc_time.tm_min,
           rtc_time.tm_sec);
    
    // 日期部分
    printf("\n日期信息:\n");
    printf("  日期: %04d-%02d-%02d\n",
           rtc_time.tm_year + 1900,
           rtc_time.tm_mon + 1,
           rtc_time.tm_mday);
    
    // 时间部分
    printf("\n时间信息:\n");
    printf("  时间: %02d:%02d:%02d\n",
           rtc_time.tm_hour+8,
           rtc_time.tm_min,
           rtc_time.tm_sec);
    
    // 12小时制时间
    int hour12 = rtc_time.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    printf("  12小时制: %02d:%02d:%02d %s\n",
           hour12,
           rtc_time.tm_min,
           rtc_time.tm_sec,
           rtc_time.tm_hour >= 12 ? "PM" : "AM");
    
    // 星期
    const char *weekdays[] = {"星期日", "星期一", "星期二", "星期三", 
                             "星期四", "星期五", "星期六"};
    printf("\n其他信息:\n");
    printf("  星期: %s\n", weekdays[rtc_time.tm_wday]);
    
    // 一年中的第几天
    printf("  本年第 %d 天\n", rtc_time.tm_yday + 1);
    
    // 季度信息
    int quarter = (rtc_time.tm_mon / 3) + 1;
    printf("  第 %d 季度\n", quarter);
    
    // 月份天数统计（简单版）
    const int month_days[] = {31, 28, 31, 30, 31, 30, 
                             31, 31, 30, 31, 30, 31};
    int days_in_month = month_days[rtc_time.tm_mon];
    // 闰年二月调整
    if (rtc_time.tm_mon == 1) {
        int year = rtc_time.tm_year + 1900;
        if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) {
            days_in_month = 29;
        }
    }
    printf("  本月共 %d 天，剩余 %d 天\n", 
           days_in_month, 
           days_in_month - rtc_time.tm_mday);
    
    return 0;
}


// entry.S needs one stack per CPU.
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

volatile static int started = 0;

void
printlogo()
{
  printf("\n");
  printf("███████╗ ██╗  ██╗ ██╗  ██╗  ██████╗  ███████╗\n");
  printf("╚══███╔╝ ╚██╗██╔╝ ╚██╗██╔╝ ██╔═══██╗ ██╔════╝\n");
  printf("  ███╔╝   ╚███╔╝   ╚███╔╝  ██║   ██║ ███████╗\n");
  printf(" ███╔╝    ██╔██╗   ██╔██╗  ██║   ██║ ╚════██║\n");
  printf("███████╗ ██╔╝ ██╗ ██╔╝ ██╗ ╚██████╔╝ ███████║\n");
  printf("╚══════╝ ╚═╝  ╚═╝ ╚═╝  ╚═╝  ╚═════╝  ╚══════╝\n");
  printf("\n");
}

// entry.S jumps here on stack0.
void
main()
{
   if(cpuid() == 0){
    consoleinit();
    printfinit();
    printlogo();
    kinit();         // physical page allocator
//printf("kinit\n");
    vminit();        // create kernel page table
//printf("vminit\n");
    procinit();      // process table
//printf("procinit\n");
    trapinit();      // trap vectors
//printf("trapinit\n");
    apic_init();     // set up LS7A1000 interrupt controller
//printf("apicinit\n");
    extioi_init();   // extended I/O interrupt controller
//printf("extioi_init\n");
    binit();         // buffer cache
//printf("binit\n");
    iinit();         // inode table
//printf("iinit\n");
    fileinit();      // file table
//printf("fileinit\n");
    sem_init();
    ramdiskinit();   // emulated hard disk
    rtc_init();
    rtc_print_date();
//printf("ramdiskinit\n");
    userinit();      // first user process
//printf("userinit\n");
    __sync_synchronize();
    started = 1;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
  }
    scheduler(); 
}

