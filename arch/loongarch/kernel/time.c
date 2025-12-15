#include "types.h"
#include "loongarch.h"
#include "time.h"
#include "defs.h"


// 解析TOY_READ0寄存器的位域
#define TOY_MSEC_MASK  0x0000000f  // 位3:0 - 毫秒
#define TOY_SEC_MASK   0x000003f0  // 位9:4 - 秒 (0-59)
#define TOY_MIN_MASK   0x0000fc00  // 位15:10 - 分 (0-59)
#define TOY_HOUR_MASK  0x001f0000  // 位20:16 - 时 (0-23)
#define TOY_DAY_MASK   0x03e00000  // 位25:21 - 日 (1-31)
#define TOY_MON_MASK   0xfc000000  // 位31:26 - 月 (1-12)

// 提取位域的宏
#define EXTRACT_SEC(x)  (((x) & TOY_SEC_MASK) >> 4)
#define EXTRACT_MIN(x)  (((x) & TOY_MIN_MASK) >> 10)
#define EXTRACT_HOUR(x) (((x) & TOY_HOUR_MASK) >> 16)
#define EXTRACT_DAY(x)  (((x) & TOY_DAY_MASK) >> 21)
#define EXTRACT_MON(x)  (((x) & TOY_MON_MASK) >> 26)
#define EXTRACT_MSEC(x) ((x) & TOY_MSEC_MASK)

/**
 * @brief 计算星期几 (Zeller's congruence)
 * @param year 年份（如2025）
 * @param month 月份 1-12
 * @param day 日期 1-31
 * @return 星期几 0=周日,1=周一,...,6=周六
 */
static int 
calculate_weekday(int year, int month, int day)
{
    if (month < 3) {
        month += 12;
        year -= 1;
    }
    
    int k = year % 100;
    int j = year / 100;
    
    // Zeller's congruence
    int h = (day + (13 * (month + 1)) / 5 + k + k/4 + j/4 + 5*j) % 7;
    
    // 转换为 0=周日,1=周一,...,6=周六
    return (h + 6) % 7;
}

/**
 * @brief 计算一年中的第几天
 */
static int 
calculate_yearday(int year, int month, int day)
{
    const int month_days[] = {31, 28, 31, 30, 31, 30,
                              31, 31, 30, 31, 30, 31};
    const int leap_month_days[] = {31, 29, 31, 30, 31, 30,
                                   31, 31, 30, 31, 30, 31};
    
    // 判断闰年
    int is_leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
    const int *days_array = is_leap ? leap_month_days : month_days;
    
    int yearday = day;
    for (int i = 0; i < month - 1; i++) {
        yearday += days_array[i];
    }
    
    return yearday - 1;  // tm_yday是0-365
}

int
localtime_r(uint64_t raw_data, struct tm *tm)
{
    if (tm == NULL) {
        return -1;
    }
    
    // 分离高32位和低32位
    uint32_t toy0 = (uint32_t)(raw_data & 0xFFFFFFFF);
    uint32_t toy1 = (uint32_t)(raw_data >> 32);
    
    // 解析各个字段
    int sec   = (toy0 >> 4) & 0x3f;
    int min   = (toy0 >> 10) & 0x3f;
    int hour  = (toy0 >> 16) & 0x1f;
    int day   = (toy0 >> 21) & 0x1f;
    int month = (toy0 >> 26) & 0x3f;
    int year  = toy1 + 1900;  // RTC年份是从1900年开始的
    
    
    // 填充tm结构体
    tm->tm_sec   = sec;
    tm->tm_min   = min;
    tm->tm_hour  = hour;
    tm->tm_mday  = day;
    tm->tm_mon   = month - 1;  // struct tm月份是0-11
    tm->tm_year  = year - 1900;  // struct tm年份从1900开始
    tm->tm_wday  = calculate_weekday(year, month, day);
    tm->tm_yday  = calculate_yearday(year, month, day);
    tm->tm_isdst = 0;  // RTC不存储夏令时信息
    
    return 0;
}
