# 在龙芯QEMU虚拟机中定位RTC物理地址并正确读取时间的完整过程

## 引言：探索龙芯RTC定位的困境

在开发基于龙芯架构的操作系统时，我发现了一个有趣的技术挑战：如何在QEMU模拟的龙芯虚拟机中找到实时时钟（RTC）的物理地址并正确读取时间？与x86或ARM架构不同，龙芯的RTC配置在网上几乎没有现成的文档说明，这促使我进行了一次技术探索之旅。

## 第一阶段：网络资料的匮乏

最初，我在网上搜索"龙芯 RTC 地址"、"LoongArch RTC"等相关关键词，但发现相关资料极为有限。大多数关于龙芯的讨论集中在CPU架构和性能优化上，关于外设地址映射的具体信息几乎没有公开的详细文档。这与x86架构形成鲜明对比，x86的RTC有标准化的CMOS接口和明确的端口地址（0x70/0x71）。

尝试的搜索路径：
1. 龙芯官方技术文档 - 需要注册且不对外公开
2. GitHub上的开源项目 - 大多是应用层代码
3. 技术论坛和社区 - 讨论偏理论，缺乏实操细节

## 第二阶段：Linux内核源码的探索与适配问题

既然公开资料有限，我转向了Linux内核源码。在`drivers/rtc/`目录下找到了龙芯相关的RTC驱动程序：

```c
// drivers/rtc/rtc-loongson.c
static const struct of_device_id loongson_rtc_of_match[] = {
    { .compatible = "loongson,ls7a-rtc", .data = &generic_rtc_config },
    // ...
};
```

从代码中可以看出，龙芯使用设备树（Device Tree）来描述硬件，RTC的地址信息应该从设备树中获取。然而，这带来了新的问题：

1. **地址不确定性**：设备树中的地址是动态分配的
2. **QEMU差异**：实际的QEMU模拟环境可能与Linux支持的硬件不同
3. **寄存器差异**：不同龙芯型号的RTC寄存器布局可能不同

在Linux驱动中，我发现了关键的寄存器定义：
```c
#define TOY_READ0_REG    0x2c  // 时间低32位
#define TOY_READ1_REG    0x30  // 时间高32位
```

但这些寄存器偏移是否正确？需要在实际环境中验证。

## 第三阶段：QEMU设备树的突破性发现

既然Linux源码无法直接提供确切地址，我决定直接从QEMU获取硬件信息。通过以下命令导出QEMU的设备树：

```bash
qemu-system-loongarch64 -M virt -kernel vmlinux -machine dumpdtb=qemu.dtb
dtc -I dtb -O dts qemu.dtb -o qemu.dts
```

分析导出的设备树文件，我找到了关键信息：
```dts
rtc@100d0100 {
    compatible = "loongson,ls7a-rtc";
    reg = <0x00 0x100d0100 0x00 0x100>;
};
```

**重要发现**：
1. RTC物理地址：`0x100d0100`（不是常见的`0x1fe0d000`）
2. 兼容性字符串：`loongson,ls7a-rtc`
3. 地址范围：256字节（0x100）

这与我在网上看到的任何资料都不同，也说明了为什么直接使用Linux源码中的地址会失败。

## 第四阶段：设备初始化与寄存器使能

有了正确的地址，还需要正确初始化RTC。通过分析寄存器内容，我发现了一个关键问题：

```c
// 初始化前读取控制寄存器
uint32_t ctrl = *(volatile uint32_t*)(RTC + 0x40);
printf("控制寄存器: 0x%08x\n", ctrl);  // 输出: 0x00000000
```

RTC控制寄存器为0，意味着RTC可能未启用。查阅龙芯7A手册（非公开），我找到了关键的使能位：

```c
// 使能RTC
#define TOY_ENABLE   0x800  // bit 11
#define OSC_ENABLE   0x100  // bit 8

*(volatile uint32_t*)(RTC + 0x40) |= (TOY_ENABLE | OSC_ENABLE);
```

初始化后再次读取：
```c
ctrl = *(volatile uint32_t*)(RTC + 0x40);
printf("设置后控制寄存器: 0x%08x\n", ctrl);  // 输出: 0x00000900
```

现在RTC已经可以正常读取数据了。

## 第五阶段：时间解码与架构差异

### 与RISC-V架构的区别

在RISC-V架构中，时间通常通过`time` CSR寄存器或内存映射的CLINT设备获取，时间是连续的64位计数器。而龙芯的RTC设计完全不同：

**RISC-V时间读取**：
```c
// RISC-V通常这样获取时间
uint64_t read_time(void) {
    uint64_t time;
    asm volatile("rdtime %0" : "=r"(time));
    return time;
}
```

**龙芯RTC时间解码**：
```c
// 龙芯需要解析复杂的位域结构
uint32_t toy0 = *(volatile uint32_t*)(RTC + 0x2c);
uint32_t toy1 = *(volatile uint32_t*)(RTC + 0x30);

int sec   = (toy0 >> 4) & 0x3f;    // 位9:4
int min   = (toy0 >> 10) & 0x3f;   // 位15:10
int hour  = (toy0 >> 16) & 0x1f;   // 位20:16
int day   = (toy0 >> 21) & 0x1f;   // 位25:21
int month = (toy0 >> 26) & 0x3f;   // 位31:26
int year  = toy1;                  // 完整32位年份
```

这种位域编码方式比RISC-V的时间计数器复杂得多，但提供了更丰富的时间信息。

### UTC时间转换的关键发现

在测试过程中，我发现了一个有趣的现象：读取的RTC时间比实际时间慢了8小时。

**问题现象**：
- RTC读取时间：14:40:54
- 实际系统时间：22:42:xx
- 差值：约8小时

**原因分析**：
查看QEMU启动参数：
```bash
qemu-system-loongarch64 ... -rtc base=utc
```

RTC存储的是UTC时间，而我的系统显示的是本地时间（UTC+8）。这解释了8小时的差值。

**解决方案**：
```c
// 转换为本地时间（UTC+8）
int local_hour = hour + 8;
if (local_hour >= 24) {
    local_hour -= 24;
    // 处理跨日逻辑
}
```

或者在QEMU启动时使用本地时间基准：
```bash
qemu-system-loongarch64 ... -rtc base=localtime
```

最终的完整解决方案：
```c

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
struct tm {
  int tm_sec;   // 0-60
  int tm_min;   // 0-59
  int tm_hour;  // 0-23
  int tm_mday;  // 1-31
  int tm_mon;   // 0-11
  int tm_year;  // since 1900
  int tm_wday;  // 0-6
  int tm_yday;  // 0-365
  int tm_isdst; // zero
};
```

