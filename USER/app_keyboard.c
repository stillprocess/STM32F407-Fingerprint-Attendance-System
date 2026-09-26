#include "app_keyboard.h"
#include "app_fingerprint.h"
#include "app_oled.h"
#include "app_servo.h"
#include "FreeRTOS.h"
#include "RTC.h"
#include "keyboard.h"
#include "sha256.h"
#include "stdio.h"
#include "stm32f4xx.h"
#include "string.h"
#include "task.h"
#include "uart.h"
#include "w25q128.h"

#define PASSWORD_LENGTH 6U
#define ADMIN_ACCESS_MS 60000U

typedef struct
{
    char data[PASSWORD_LENGTH + 1U];
    uint8_t count;
} KEYBuffer;

static TickType_t admin_deadline; // 授权到期时刻
static uint8_t admin_active;      // 1 表示已通过 D 键验证

// 检查一分钟管理权限；到期后清除授权
static BaseType_t Admin_CheckAccess(void)
{
    BaseType_t allowed;
    TickType_t now;

    allowed = pdFALSE;
    // 键盘任务和指纹任务共用授权状态
    taskENTER_CRITICAL();
    now = xTaskGetTickCount();
    if(admin_active != 0U)
    {
        // 有符号差值处理 Tick 回绕
        if((int32_t)(admin_deadline - now) > 0)
        {
            allowed = pdTRUE;
        }
        else
        {
            admin_active = 0U;
        }
    }
    taskEXIT_CRITICAL();

    return allowed;
}

// 查询当前管理权限，不消耗使用次数
BaseType_t Admin_HasAccess(void)
{
    return Admin_CheckAccess();
}

// 原密码验证成功后，开启一分钟管理权限
static void Admin_GrantAccess(void)
{
    taskENTER_CRITICAL();
    admin_deadline = xTaskGetTickCount() + pdMS_TO_TICKS(ADMIN_ACCESS_MS);
    admin_active = 1U;
    taskEXIT_CRITICAL();
}

// 验证失败时撤销管理权限
static void Admin_RevokeAccess(void)
{
    taskENTER_CRITICAL();
    admin_active = 0U;
    taskEXIT_CRITICAL();
}

// 密码区全为 0xFF 时视为首次设置；此处不检测 Flash 是否在线
static BaseType_t Password_IsSet(void)
{
    uint8_t saved_hash[32];
    uint8_t i;

    W25Q128_Read(0x000000U, saved_hash, sizeof(saved_hash));
    for(i = 0U; i < sizeof(saved_hash); i++)
    {
        if(saved_hash[i] != 0xFFU)
        {
            return pdTRUE;
        }
    }

    return pdFALSE;
}

// 比较输入密码和 Flash 中的 SHA-256 摘要
static BaseType_t Password_Matches(const char *password)
{
    uint8_t saved_hash[32];
    uint8_t input_hash[32];

    Password_Hash(password, input_hash);
    W25Q128_Read(0x000000U, saved_hash, sizeof(saved_hash));
    return (memcmp(saved_hash, input_hash, sizeof(saved_hash)) == 0) ? pdTRUE : pdFALSE;
}

// 等待矩阵键盘松开，避免一次按下重复执行
static void Keyboard_WaitRelease(void)
{
    while(get_key_board() != 'N')
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// 清空本次输入和星号计数
static void Password_Reset(KEYBuffer *keybuf, uint8_t *star_count)
{
    memset(keybuf->data, 0, sizeof(keybuf->data));
    keybuf->count = 0;
    *star_count = 0;
}

// 处理矩阵键盘的密码、考勤和管理操作
void TaskKeyboardUnlock(void *pvParameters)
{
    char key;
    uint8_t star_count;
    uint8_t clear_confirm_pending;
    uint8_t hash[32];
    uint8_t input_hash[32];
    KEYBuffer keybuf;
    FingerprintResult_t fingerprint_result;
    AttendanceRecord_t attendance_record;
    RTC_TimeTypeDef attendance_time;
    RTC_DateTypeDef attendance_date;
    int32_t attendance_index;
    char id_buf[20];
    char date_buf[20];
    char time_buf[20];
    char serial_buf[64];

    (void)pvParameters;

    star_count = 0;
    clear_confirm_pending = 0;
    memset(&keybuf, 0, sizeof(keybuf));

    // 键盘任务统一处理密码和 A/B/C/D 菜单操作
    USART1_SendString("Enter password\r\n");
    OLED_ShowMainMenu();

    for( ;; )
    {
        key = get_key_board();
        if(key == 'N')
        {
            // 授权到期后退出清空确认页
            if(clear_confirm_pending != 0 && Admin_HasAccess() != pdTRUE)
            {
                clear_confirm_pending = 0;
                OLED_ShowMainMenu();
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if(clear_confirm_pending != 0 && key != 'C')
        {
            // 第二次输入不是 C 时取消清空确认
            clear_confirm_pending = 0;
            USART1_SendString("Clear cancelled\r\n");
            OLED_ShowMainMenu();
        }

        if(key == 'A')
        {
            // A 键请求指纹任务完成考勤验证
            Password_Reset(&keybuf, &star_count);
            Keyboard_WaitRelease();

            // RTC 不可用时不写入考勤时间
            if(RTC_IsReady() == 0U)
            {
                OLED_ShowMenuLines("RTC error", "Attendance off", NULL, NULL);
                vTaskDelay(pdMS_TO_TICKS(2000));
                OLED_ShowMainMenu();
                continue;
            }

            OLED_ShowMenuLines("Place finger", NULL, NULL, NULL);

            // 键盘任务等待验证结果，指纹串口仍只由指纹任务访问
            if(Fingerprint_AttendanceVerify(&fingerprint_result, pdMS_TO_TICKS(100), portMAX_DELAY) != pdPASS)
            {
                OLED_ShowMenuLines("FPM busy", NULL, NULL, NULL);
            }
            else if(fingerprint_result.success == 0)
            {
                OLED_ShowMenuLines("Finger failed", NULL, NULL, NULL);
                USART1_SendString("Attendance failed\r\n");
            }
            else
            {
                // RTC 读取顺序为时间、日期
                RTC_GetTime(RTC_Format_BIN, &attendance_time);
                RTC_GetDate(RTC_Format_BIN, &attendance_date);

                // 0xFF 与 Flash 擦除态一致，magic 用于判断记录是否有效
                memset(&attendance_record, 0xFF, sizeof(attendance_record));
                attendance_record.magic = ATTENDANCE_RECORD_MAGIC;
                attendance_record.finger_id = fingerprint_result.finger_id;
                attendance_record.year = attendance_date.RTC_Year;
                attendance_record.month = attendance_date.RTC_Month;
                attendance_record.day = attendance_date.RTC_Date;
                attendance_record.hour = attendance_time.RTC_Hours;
                attendance_record.minute = attendance_time.RTC_Minutes;
                attendance_record.second = attendance_time.RTC_Seconds;

                attendance_index = Attendance_SaveRecord(&attendance_record);
                if(attendance_index >= 0)
                {
                    sprintf(id_buf, "ID%u OK", (unsigned int)fingerprint_result.finger_id);
                    sprintf(date_buf, "20%02u-%02u-%02u", attendance_record.year, attendance_record.month, attendance_record.day);
                    sprintf(time_buf, "%02u:%02u:%02u", attendance_record.hour, attendance_record.minute, attendance_record.second);
                    OLED_ShowMenuLines("Attendance OK", id_buf, date_buf, time_buf);

                    sprintf(serial_buf, "Attendance ID:%u No:%u\r\n", (unsigned int)fingerprint_result.finger_id, (unsigned int)(attendance_index + 1));
                    USART1_SendString(serial_buf);
                }
                else
                {
                    OLED_ShowMenuLines("Records full", NULL, NULL, NULL);
                    USART1_SendString("Records full\r\n");
                }
            }

            vTaskDelay(pdMS_TO_TICKS(2000));
            OLED_ShowMainMenu();
            continue;
        }

        if(key == 'B')
        {
            // B 键显示本地考勤记录
            Password_Reset(&keybuf, &star_count);
            Keyboard_WaitRelease();
            OLED_ShowAttendanceRecords();
            continue;
        }

        if(key == 'C')
        {
            // C 键需要连续确认两次才擦除记录
            Password_Reset(&keybuf, &star_count);
            Keyboard_WaitRelease();

            if(clear_confirm_pending == 0)
            {
                if(Admin_HasAccess() != pdTRUE)
                {
                    OLED_ShowMenuLines(NULL, "Admin required", "6 digits + D", NULL);
                    USART1_SendString("Admin required\r\n");
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    OLED_ShowMainMenu();
                    continue;
                }

                clear_confirm_pending = 1;
                OLED_ShowMenuLines("Clear records?", "Press C again", NULL, NULL);
                USART1_SendString("Press C again\r\n");
                continue;
            }

            clear_confirm_pending = 0;
            if(Admin_HasAccess() != pdTRUE)
            {
                OLED_ShowMenuLines(NULL, "Admin expired", NULL, NULL);
                USART1_SendString("Admin expired\r\n");
                vTaskDelay(pdMS_TO_TICKS(2000));
                OLED_ShowMainMenu();
                continue;
            }

            OLED_ShowMenuLines(NULL, "Clearing...", NULL, NULL);
            if(Attendance_ClearRecords() == 0)
            {
                OLED_ShowMenuLines(NULL, "Records cleared", NULL, NULL);
                USART1_SendString("Records cleared\r\n");
            }
            else
            {
                OLED_ShowMenuLines(NULL, "Clear failed", NULL, NULL);
                USART1_SendString("Clear failed\r\n");
            }

            vTaskDelay(pdMS_TO_TICKS(2000));
            OLED_ShowMainMenu();
            continue;
        }

        if(key >= '0' && key <= '9')
        {
            if(keybuf.count < PASSWORD_LENGTH)
            {
                keybuf.data[keybuf.count] = key;
                keybuf.count++;
            }
            else
            {
                // 超过六位时只保留最后输入的六位
                memmove(keybuf.data, &keybuf.data[1], PASSWORD_LENGTH - 1U);
                keybuf.data[PASSWORD_LENGTH - 1U] = key;
            }

            keybuf.data[PASSWORD_LENGTH] = '\0';
            if(star_count < PASSWORD_LENGTH)
            {
                USART1_SendString("*");
                OLED_ShowPasswordStar(star_count);
                star_count++;
            }

            Keyboard_WaitRelease();
            continue;
        }

        if(key == '*')
        {
            // 首次设置免授权，修改已有密码先用 D 验证原密码
            if(keybuf.count != PASSWORD_LENGTH)
            {
                OLED_ShowMenuLines(NULL, "Enter 6 digits", NULL, NULL);
            }
            else if(Password_IsSet() == pdTRUE && Admin_HasAccess() != pdTRUE)
            {
                OLED_ShowMenuLines(NULL, "Admin required", "6 digits + D", NULL);
                USART1_SendString("Admin required\r\n");
            }
            else
            {
                Password_Hash(keybuf.data, hash);
                Password_Save(0x000000U, hash, sizeof(hash));
                // 写入后回读，确认密码摘要已保存
                W25Q128_Read(0x000000U, input_hash, sizeof(input_hash));
                if(memcmp(hash, input_hash, sizeof(hash)) == 0)
                {
                    USART1_SendString("\r\nPassword saved\r\n");
                    OLED_ShowMenuLines(NULL, "Password saved", NULL, NULL);
                }
                else
                {
                    USART1_SendString("\r\nPassword save failed\r\n");
                    OLED_ShowMenuLines(NULL, "Password save failed", NULL, NULL);
                }
            }

            vTaskDelay(pdMS_TO_TICKS(2000));
            OLED_ShowMainMenu();
            Password_Reset(&keybuf, &star_count);
            Keyboard_WaitRelease();
            continue;
        }

        if(key == '#')
        {
            // 井号仅用于开锁，不授予管理权限
            if(keybuf.count != PASSWORD_LENGTH)
            {
                OLED_ShowMenuLines(NULL, "Enter 6 digits", NULL, NULL);
            }
            else
            {
                if(Password_Matches(keybuf.data) == pdTRUE)
                {
                    USART1_SendString("Password OK\r\n");
                    // 舵机任务独立管理十秒保持和自动上锁
                    Servo_RequestUnlock(pdMS_TO_TICKS(10000));
                    OLED_ShowMenuLines(NULL, "Password correct", NULL, NULL);
                }
                else
                {
                    USART1_SendString("Password error\r\n");
                    OLED_ShowMenuLines(NULL, "Password incorrect", NULL, NULL);
                }
            }

            vTaskDelay(pdMS_TO_TICKS(2000));
            OLED_ShowMainMenu();
            Password_Reset(&keybuf, &star_count);
            Keyboard_WaitRelease();
            continue;
        }

        if(key == 'D')
        {
            // D 键验证原密码，管理权限保持一分钟
            if(keybuf.count != PASSWORD_LENGTH)
            {
                Admin_RevokeAccess();
                OLED_ShowMenuLines(NULL, "Enter 6 digits", NULL, NULL);
            }
            else if(Password_Matches(keybuf.data) == pdTRUE)
            {
                Admin_GrantAccess();
                OLED_ShowMenuLines(NULL, "Admin 60s", "Access granted", NULL);
                USART1_SendString("Admin granted\r\n");
            }
            else
            {
                Admin_RevokeAccess();
                OLED_ShowMenuLines(NULL, "Admin denied", NULL, NULL);
                USART1_SendString("Admin denied\r\n");
            }

            vTaskDelay(pdMS_TO_TICKS(2000));
            OLED_ShowMainMenu();
            Password_Reset(&keybuf, &star_count);
            Keyboard_WaitRelease();
            continue;
        }

        Keyboard_WaitRelease();
    }
}
