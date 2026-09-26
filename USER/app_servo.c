#include "app_servo.h"
#include "queue.h"
#include "servo.h"
#include "task.h"
#include "uart.h"

// 长度为 1，新的开锁请求覆盖旧等待值
static QueueHandle_t ServoQueue = NULL;

// 初始化舵机 PWM，并创建开锁请求队列
BaseType_t ServoApp_Init(void)
{
    Servo_PWM_Init();
    Servo_SetAngle(SERVO_LOCK_ANGLE);

    ServoQueue = xQueueCreate(1, sizeof(TickType_t));
    return (ServoQueue != NULL) ? pdPASS : pdFAIL;
}

// 发送开锁时长；新请求覆盖尚未处理的旧请求
BaseType_t Servo_RequestUnlock(TickType_t hold_ticks)
{
    if(ServoQueue == NULL || hold_ticks == 0)
    {
        return pdFAIL;
    }

    return xQueueOverwrite(ServoQueue, &hold_ticks);
}

// 开锁后等待超时，再自动上锁
void TaskServo(void *pvParameters)
{
    TickType_t hold_ticks;
    TickType_t deadline;
    TickType_t now;
    TickType_t remaining;

    (void)pvParameters;

    for( ;; )
    {
        if(xQueueReceive(ServoQueue, &hold_ticks, portMAX_DELAY) != pdPASS)
        {
            continue;
        }

        Servo_SetAngle(SERVO_UNLOCK_ANGLE);
        USART1_SendString("Unlock\r\n");
        deadline = xTaskGetTickCount() + hold_ticks;

        for( ;; )
        {
            now = xTaskGetTickCount();
            // 有符号差值可处理 Tick 计数回绕
            if((int32_t)(deadline - now) <= 0)
            {
                break;
            }

            remaining = deadline - now;
            if(xQueueReceive(ServoQueue, &hold_ticks, remaining) == pdPASS)
            {
                // 开锁期间再次验证成功时重新计算十秒保持时间
                deadline = xTaskGetTickCount() + hold_ticks;
            }
            else
            {
                break;
            }
        }

        Servo_SetAngle(SERVO_LOCK_ANGLE);
        USART1_SendString("Lock\r\n");
    }
}
