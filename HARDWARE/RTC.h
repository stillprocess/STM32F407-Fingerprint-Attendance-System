#ifndef __RTC_H__
#define __RTC_H__

#include <stdint.h>

void RTC_Config(void);       // 初始化 RTC
uint8_t RTC_IsReady(void);   // RTC 时间是否可用
void RTC_ShowTimeDate(void); // 在 OLED 显示日期和时间

#endif
