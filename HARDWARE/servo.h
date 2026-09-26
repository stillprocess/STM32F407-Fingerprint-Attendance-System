#ifndef _SERVO_H
#define _SERVO_H

#include <stdint.h>

#define SERVO_LOCK_ANGLE      0
#define SERVO_UNLOCK_ANGLE    90

void Servo_PWM_Init(void);            // 初始化舵机 PWM
void Servo_SetAngle(uint16_t angle);  // 设置舵机角度

#endif
