#ifndef __APP_SERVO_H__
#define __APP_SERVO_H__

#include "FreeRTOS.h"

BaseType_t ServoApp_Init(void); // 初始化 PWM 和开锁队列
BaseType_t Servo_RequestUnlock(TickType_t hold_ticks); // 请求开锁并指定保持时长
void TaskServo(void *pvParameters); // 开锁和自动上锁任务

#endif
