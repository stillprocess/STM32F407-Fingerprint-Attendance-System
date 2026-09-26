#include "app_fingerprint.h"
#include "app_keyboard.h"
#include "app_oled.h"
#include "app_servo.h"
#include "FPM383F.h"
#include "beep.h"
#include "event_groups.h"
#include "key.h"
#include "queue.h"
#include "stdio.h"
#include "stm32f4xx.h"
#include "task.h"
#include "uart.h"

typedef enum
{
    FINGERPRINT_COMMAND_ENROLL = 1, // 录入
    FINGERPRINT_COMMAND_VERIFY,
    FINGERPRINT_COMMAND_TOTAL,
    FINGERPRINT_COMMAND_CLEAR,
    FINGERPRINT_COMMAND_ATTENDANCE // A 键考勤，不开锁
} FingerprintCommand_t;

EventGroupHandle_t KeyEventGroup = NULL; // EXTI 通知按键任务

// 命令队列保证指纹操作按顺序执行
static QueueHandle_t FingerprintCommandQueue = NULL;

// 结果队列只返回 A 键考勤验证结果
static QueueHandle_t FingerprintResultQueue = NULL;

// 创建按键事件组、指纹命令队列和考勤结果队列
BaseType_t FingerprintApp_Init(void)
{
    KeyEventGroup = xEventGroupCreate();
    if(KeyEventGroup == NULL)
    {
        return pdFAIL;
    }

    FingerprintCommandQueue = xQueueCreate(4, sizeof(FingerprintCommand_t));
    if(FingerprintCommandQueue == NULL)
    {
        return pdFAIL;
    }

    FingerprintResultQueue = xQueueCreate(1, sizeof(FingerprintResult_t));
    if(FingerprintResultQueue == NULL)
    {
        return pdFAIL;
    }

    return pdPASS;
}

// A 键发送考勤命令，并等待指纹任务返回匹配结果
BaseType_t Fingerprint_AttendanceVerify(FingerprintResult_t *result, TickType_t send_timeout, TickType_t result_timeout)
{
    FingerprintCommand_t command = FINGERPRINT_COMMAND_ATTENDANCE;

    if(result == NULL || FingerprintCommandQueue == NULL || FingerprintResultQueue == NULL)
    {
        return pdFAIL;
    }

    if(xQueueSend(FingerprintCommandQueue, &command, send_timeout) != pdPASS)
    {
        return pdFAIL;
    }

    // 请求和结果分离，调用者不直接访问 USART2
    return xQueueReceive(FingerprintResultQueue, result, result_timeout);
}

// EXTI 只产生事件，任务中完成消抖、松手检测和命令发送
static void FPM_HandleKey(IRQn_Type irq, uint32_t exti_line, GPIO_TypeDef *port, uint16_t pin, FingerprintCommand_t command, const char *key_name)
{
    char send_buf[32];

    // 消抖期间关闭当前按键中断，避免同一次按下重复产生事件
    NVIC_DisableIRQ(irq);
    vTaskDelay(pdMS_TO_TICKS(20));

    if(GPIO_ReadInputDataBit(port, pin) == Bit_RESET)
    {
        sprintf(send_buf, "%s Press\r\n", key_name);
        USART1_SendString(send_buf);

        while(GPIO_ReadInputDataBit(port, pin) == Bit_RESET)
        {
            vTaskDelay(pdMS_TO_TICKS(20));
        }

        if((command == FINGERPRINT_COMMAND_ENROLL || command == FINGERPRINT_COMMAND_CLEAR) && Admin_HasAccess() != pdTRUE)
        {
            OLED_ShowMenuLines(NULL, "Admin required", "6 digits + D", NULL);
            USART1_SendString("Admin required\r\n");
            vTaskDelay(pdMS_TO_TICKS(2000));
            OLED_ShowMainMenu();
        }
        else if(xQueueSend(FingerprintCommandQueue, &command, 0) != pdPASS)
        {
            USART1_SendString("Fingerprint command queue full\r\n");
        }
    }

    EXTI_ClearITPendingBit(exti_line);
    NVIC_ClearPendingIRQ(irq);
    NVIC_EnableIRQ(irq);
}

// 指纹操作成功时短响一次
static void FPM_BeepSuccess(void)
{
    beep_ON();
    vTaskDelay(pdMS_TO_TICKS(50));
    beep_OFF();
}

// 将 S1～S4 的 EXTI 事件转成指纹操作命令
void app_task_key(void *pvParameters)
{
    EventBits_t event_value;

    (void)pvParameters;
    key_init();

    for( ;; )
    {
        event_value = xEventGroupWaitBits(KeyEventGroup, EVENT_GROUP_KEY_ALL, pdTRUE, pdFALSE, portMAX_DELAY);

        if((event_value & EVENT_GROUP_KEY1_DOWN) != 0)
        {
            FPM_HandleKey(EXTI0_IRQn, EXTI_Line0, GPIOA, GPIO_Pin_0, FINGERPRINT_COMMAND_ENROLL, "S1");
        }
        if((event_value & EVENT_GROUP_KEY2_DOWN) != 0)
        {
            FPM_HandleKey(EXTI2_IRQn, EXTI_Line2, GPIOE, GPIO_Pin_2, FINGERPRINT_COMMAND_VERIFY, "S2");
        }
        if((event_value & EVENT_GROUP_KEY3_DOWN) != 0)
        {
            FPM_HandleKey(EXTI3_IRQn, EXTI_Line3, GPIOE, GPIO_Pin_3, FINGERPRINT_COMMAND_TOTAL, "S3");
        }
        if((event_value & EVENT_GROUP_KEY4_DOWN) != 0)
        {
            FPM_HandleKey(EXTI4_IRQn, EXTI_Line4, GPIOE, GPIO_Pin_4, FINGERPRINT_COMMAND_CLEAR, "S4");
        }
    }
}

// 指纹任务是唯一访问 FPM383F 和 USART2 的业务任务
void app_task_fpm(void *pvParameters)
{
    FingerprintCommand_t command;
    FingerprintResult_t fingerprint_result;
    uint16_t id;
    uint16_t id_total;
    int32_t fmp_error_code;
    char send_buf[64];

    (void)pvParameters;

    fpm_init();
    USART1_SendString("FPM383F ready\r\n");

    for( ;; )
    {
        if(xQueueReceive(FingerprintCommandQueue, &command, portMAX_DELAY) != pdPASS)
        {
            continue;
        }

        // 排队期间授权可能过期，执行前再次检查
        if((command == FINGERPRINT_COMMAND_ENROLL || command == FINGERPRINT_COMMAND_CLEAR) && Admin_HasAccess() != pdTRUE)
        {
            OLED_ShowMenuLines(NULL, "Admin expired", NULL, NULL);
            USART1_SendString("Admin expired\r\n");
            vTaskDelay(pdMS_TO_TICKS(2000));
            OLED_ShowMainMenu();
            continue;
        }

        if(command == FINGERPRINT_COMMAND_ENROLL)
        {
            fpm_ctrl_led(FPM_LED_BLUE);
            USART1_SendString("Enroll finger\r\n");

            fmp_error_code = fpm_id_total(&id_total);
            if(fmp_error_code != 0)
            {
                fpm_ctrl_led(FPM_LED_RED);
                USART1_SendString("Get finger count failed\r\n");
                continue;
            }

            if(id_total >= FPM_MAX_FINGERPRINTS)
            {
                fpm_ctrl_led(FPM_LED_RED);
                USART1_SendString("Finger library full\r\n");
                continue;
            }

            // 总数不能当作新 ID，空洞由索引表查找
            fmp_error_code = fpm_find_empty_id(&id);
            if(fmp_error_code != 0)
            {
                fpm_ctrl_led(FPM_LED_RED);
                USART1_SendString("Find empty ID failed\r\n");
                continue;
            }

            sprintf(send_buf, "Enroll ID:%u\r\n", (unsigned int)id);
            USART1_SendString(send_buf);

            fmp_error_code = fpm_enroll_auto(id);
            if(fmp_error_code == 0)
            {
                fpm_ctrl_led(FPM_LED_GREEN);
                USART1_SendString("Enroll OK\r\n");
                FPM_BeepSuccess();
            }
            else
            {
                fpm_ctrl_led(FPM_LED_RED);
                USART1_SendString("Enroll failed\r\n");
            }
        }
        else if(command == FINGERPRINT_COMMAND_VERIFY)
        {
            fpm_ctrl_led(FPM_LED_BLUE);
            USART1_SendString("Verify finger\r\n");

            // 0xFFFF 表示 1:N 全库指纹匹配
            id = 0xFFFF;
            fmp_error_code = fpm_idenify_auto(&id);

            if(fmp_error_code == 0)
            {
                fpm_ctrl_led(FPM_LED_GREEN);
                sprintf(send_buf, "Finger OK, ID:%u\r\n", (unsigned int)id);
                USART1_SendString(send_buf);
                FPM_BeepSuccess();

                // 验证任务只发送请求，舵机任务负责自动上锁
                Servo_RequestUnlock(pdMS_TO_TICKS(10000));
            }
            else
            {
                fpm_ctrl_led(FPM_LED_RED);
                USART1_SendString("Finger failed\r\n");
            }
        }
        else if(command == FINGERPRINT_COMMAND_ATTENDANCE)
        {
            fingerprint_result.success = 0;
            fingerprint_result.finger_id = 0xFFFF;

            fpm_ctrl_led(FPM_LED_BLUE);
            USART1_SendString("Attendance finger\r\n");

            // 考勤同样使用 1:N 全库匹配
            id = 0xFFFF;
            fmp_error_code = fpm_idenify_auto(&id);

            if(fmp_error_code == 0)
            {
                fpm_ctrl_led(FPM_LED_GREEN);
                FPM_BeepSuccess();
                fingerprint_result.success = 1;
                fingerprint_result.finger_id = id;
                sprintf(send_buf, "Attendance finger OK, ID:%u\r\n", (unsigned int)id);
                USART1_SendString(send_buf);
            }
            else
            {
                fpm_ctrl_led(FPM_LED_RED);
                USART1_SendString("Attendance finger failed\r\n");
            }

            // 只把匹配结果交给键盘任务，考勤记录由键盘任务保存
            xQueueSend(FingerprintResultQueue, &fingerprint_result, portMAX_DELAY);
        }
        else if(command == FINGERPRINT_COMMAND_TOTAL)
        {
            fpm_ctrl_led(FPM_LED_BLUE);
            fmp_error_code = fpm_id_total(&id_total);

            if(fmp_error_code == 0)
            {
                fpm_ctrl_led(FPM_LED_GREEN);
                sprintf(send_buf, "Finger count:%u\r\n", (unsigned int)id_total);
                USART1_SendString(send_buf);
                FPM_BeepSuccess();
            }
            else
            {
                fpm_ctrl_led(FPM_LED_RED);
                USART1_SendString("Get finger count failed\r\n");
            }
        }
        else if(command == FINGERPRINT_COMMAND_CLEAR)
        {
            fpm_ctrl_led(FPM_LED_BLUE);
            USART1_SendString("Clear fingerprints\r\n");
            fmp_error_code = fpm_empty();

            if(fmp_error_code == 0)
            {
                fpm_ctrl_led(FPM_LED_GREEN);
                USART1_SendString("Clear OK\r\n");
                FPM_BeepSuccess();
            }
            else
            {
                fpm_ctrl_led(FPM_LED_RED);
                USART1_SendString("Clear failed\r\n");
            }
        }
    }
}
