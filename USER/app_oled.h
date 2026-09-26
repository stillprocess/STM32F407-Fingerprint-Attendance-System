#ifndef __APP_OLED_H__
#define __APP_OLED_H__

#include "FreeRTOS.h"
#include <stdint.h>

BaseType_t OLED_AppInit(void); // 创建 OLED 互斥锁
void OLED_ShowMenuLines(const char *line4, const char *line5, const char *line6, const char *line7); // 刷新底部四行
void OLED_ShowPasswordStar(uint8_t index); // 显示密码星号
void OLED_ShowMainMenu(void); // 显示首页菜单
void OLED_ShowAttendanceRecords(void); // 分页显示考勤记录
void TaskOLED_ShowTime(void *pvParameters); // RTC 显示任务

#endif
