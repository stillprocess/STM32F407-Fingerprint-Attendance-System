#ifndef __KEY_H__
#define __KEY_H__

#include "FreeRTOS.h"
#include "event_groups.h"

#define EVENT_GROUP_KEY1_DOWN       (1U << 0) // S1 录入指纹
#define EVENT_GROUP_KEY2_DOWN       (1U << 1) // S2 验证指纹
#define EVENT_GROUP_KEY3_DOWN       (1U << 2) // S3 查询数量
#define EVENT_GROUP_KEY4_DOWN       (1U << 3) // S4 清空指纹
#define EVENT_GROUP_KEY_ALL         0x0FU

extern EventGroupHandle_t KeyEventGroup; // EXTI 向按键任务发送事件

void key_init(void); // 初始化独立按键和 EXTI

#endif
