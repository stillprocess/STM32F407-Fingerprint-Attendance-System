#ifndef __APP_KEYBOARD_H__
#define __APP_KEYBOARD_H__

#include "FreeRTOS.h"

BaseType_t Admin_HasAccess(void);       // 查询一分钟管理权限
void TaskKeyboardUnlock(void *pvParameters); // 矩阵键盘业务任务

#endif
