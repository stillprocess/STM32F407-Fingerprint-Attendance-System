#include "app_oled.h"
#include "FreeRTOS.h"
#include "RTC.h"
#include "oled.h"
#include "semphr.h"
#include "stdio.h"
#include "task.h"
#include "w25q128.h"

// 所有 OLED 写操作共用一个互斥锁
static SemaphoreHandle_t xOLEDMutex = NULL;

// 菜单显示期间暂停顶部时间刷新
static volatile uint8_t OLED_MenuActive = 0;

// 创建 OLED 互斥锁，供菜单和时钟任务共用
BaseType_t OLED_AppInit(void)
{
    xOLEDMutex = xSemaphoreCreateMutex();
    return (xOLEDMutex != NULL) ? pdPASS : pdFAIL;
}

// 清空并重写 OLED 底部四行；顶部时间不受影响
void OLED_ShowMenuLines(const char *line4, const char *line5, const char *line6, const char *line7)
{
    if(xSemaphoreTake(xOLEDMutex, portMAX_DELAY) == pdTRUE)
    {
        // 菜单只占用第 4～7 页，顶部日期和时间保留
        OLED_ClearPage(4);
        OLED_ClearPage(5);
        OLED_ClearPage(6);
        OLED_ClearPage(7);

        if(line4 != NULL)
        {
            OLED_ShowString(0, 4, (const uint8_t *)line4, 8);
        }
        if(line5 != NULL)
        {
            OLED_ShowString(0, 5, (const uint8_t *)line5, 8);
        }
        if(line6 != NULL)
        {
            OLED_ShowString(0, 6, (const uint8_t *)line6, 8);
        }
        if(line7 != NULL)
        {
            OLED_ShowString(0, 7, (const uint8_t *)line7, 8);
        }

        xSemaphoreGive(xOLEDMutex);
    }
}

// 显示密码输入和 A/B/C/D 操作提示
void OLED_ShowMainMenu(void)
{
    OLED_ShowMenuLines("Enter password", "A:Attendance", "B:Show records", "C:Clear D:Admin");
}

// 在密码输入行显示一位星号
void OLED_ShowPasswordStar(uint8_t index)
{
    if(index >= 6)
    {
        return;
    }

    if(xSemaphoreTake(xOLEDMutex, portMAX_DELAY) == pdTRUE)
    {
        OLED_ShowChar(90 + index * 6, 4, '*', 8);
        xSemaphoreGive(xOLEDMutex);
    }
}

// 逐条读取考勤记录，每四条显示一页
void OLED_ShowAttendanceRecords(void)
{
    AttendanceRecord_t record;
    uint16_t index;
    uint16_t shown_count;
    uint8_t page_line;
    char show_buf[24];

    OLED_MenuActive = 1;
    shown_count = 0;
    page_line = 0;

    OLED_ShowMenuLines(NULL, NULL, NULL, NULL);

    for(index = 0; index < ATTENDANCE_MAX_RECORDS; index++)
    {
        if(Attendance_ReadRecord(index, &record) == 0)
        {
            sprintf(show_buf, "ID%u OK", (unsigned int)record.finger_id);

            if(xSemaphoreTake(xOLEDMutex, portMAX_DELAY) == pdTRUE)
            {
                OLED_ShowString(0, 4 + page_line, (uint8_t *)show_buf, 8);
                xSemaphoreGive(xOLEDMutex);
            }

            shown_count++;
            page_line++;

            // OLED 底部只能显示四行，满四条后换页
            if(page_line >= 4)
            {
                vTaskDelay(pdMS_TO_TICKS(1500));

                OLED_ShowMenuLines(NULL, NULL, NULL, NULL);

                page_line = 0;
            }
        }
    }

    if(shown_count == 0)
    {
        OLED_ShowMenuLines(NULL, "Empty", NULL, NULL);
        vTaskDelay(pdMS_TO_TICKS(2000));
        OLED_ShowMainMenu();
        OLED_MenuActive = 0;
        return;
    }
    else if(page_line != 0)
    {
        vTaskDelay(pdMS_TO_TICKS(1500));
    }

    OLED_ShowMenuLines(NULL, "All show", NULL, NULL);

    vTaskDelay(pdMS_TO_TICKS(2000));
    OLED_ShowMainMenu();
    OLED_MenuActive = 0;
}

// 每秒刷新 RTC 日期和时间；写屏前取得互斥锁
void TaskOLED_ShowTime(void *pvParameters)
{
    (void)pvParameters;

    while(1)
    {
        // 菜单操作和 RTC 刷新不能同时写 OLED
        if(OLED_MenuActive == 0 && xSemaphoreTake(xOLEDMutex, portMAX_DELAY) == pdTRUE)
        {
            RTC_ShowTimeDate();
            xSemaphoreGive(xOLEDMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
