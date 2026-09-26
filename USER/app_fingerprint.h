#ifndef __APP_FINGERPRINT_H__
#define __APP_FINGERPRINT_H__

#include "FreeRTOS.h"
#include <stdint.h>

typedef struct
{
    uint8_t success;    // 1 为匹配成功
    uint16_t finger_id; // 匹配到的指纹 ID
} FingerprintResult_t;

BaseType_t FingerprintApp_Init(void); // 创建事件组和指纹队列
BaseType_t Fingerprint_AttendanceVerify(FingerprintResult_t *result, TickType_t send_timeout, TickType_t result_timeout); // 请求考勤验证并等待结果
void app_task_key(void *pvParameters); // 独立按键任务
void app_task_fpm(void *pvParameters); // 指纹模块任务

#endif
