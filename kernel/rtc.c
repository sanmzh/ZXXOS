#include "types.h"
#ifdef riscv
#include "riscv.h"
#endif
#ifdef loongarch
#include "loongarch.h"
#endif
#include "memlayout.h"
#include "defs.h"

#define RTC_TIME_LOW 0x00
#define RTC_TIME_HIGH 0x04

uint64
rtcread(void)
{
  return *(volatile uint64 *)(RTC + RTC_TIME_LOW);
}
