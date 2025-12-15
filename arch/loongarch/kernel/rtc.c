#include "types.h"
#include "loongarch.h"
#define TOY_READ0_REG 0x2c  // 低32位（秒等时间信息）
#define TOY_READ1_REG 0x30  // 高32位（年）
#include "memlayout.h"
#include "time.h"
#include "defs.h"


#define RTC_CTRL_REG 0x40
#define TOY_ENABLE   0x800  // TOY使能位
#define OSC_ENABLE   0x100  // 晶振使能

// 初始化RTC
void 
rtc_init(void) {
    volatile uint32_t *ctrl = (volatile uint32_t*)(RTC + RTC_CTRL_REG);
    uint32_t val = *ctrl;
    
    // 启用RTC
    *ctrl = val | TOY_ENABLE | OSC_ENABLE;
}


uint64 
rtcread(void)
{
    volatile uint32_t *rtc = (volatile uint32_t*)RTC;
    
    uint32_t toy0 = rtc[TOY_READ0_REG / 4];
    uint32_t toy1 = rtc[TOY_READ1_REG / 4];
    
    // 组合为64位原始值
    return ((uint64_t)toy1 << 32) | toy0;
}
