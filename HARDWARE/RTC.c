#include "RTC.h"
#include "FreeRTOS.h"
#include "oled.h"
#include "stm32f4xx.h"
#include "task.h"
#include "uart.h"
#include <stdio.h>

#define RTC_INIT_FLAG       0xA5A5U
#define RTC_LSE_TIMEOUT_MS  5000U

static RTC_TimeTypeDef RTC_TimeStructure;
static RTC_DateTypeDef RTC_DateStructure;
static uint8_t rtc_ready;

// 初始化 LSE、RTC 日历和备份域标志
void RTC_Config(void)
{
    RTC_InitTypeDef rtc_init;
    ErrorStatus init_status;
    ErrorStatus time_status;
    ErrorStatus date_status;
    uint32_t timeout = RTC_LSE_TIMEOUT_MS;
    uint8_t rtc_initialized;

    rtc_ready = 0U;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_BackupAccessCmd(ENABLE);

    rtc_initialized = (RTC_ReadBackupRegister(RTC_BKP_DR0) == RTC_INIT_FLAG);

    // 已初始化时保留日历，只重新同步 APB 影子寄存器
    if(rtc_initialized == 0U)
    {
        RCC_BackupResetCmd(ENABLE);
        RCC_BackupResetCmd(DISABLE);
        RCC_LSEConfig(RCC_LSE_ON);
    }

    // LSE 起振较慢，阻塞当前任务但不占用 CPU
    while(RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET && timeout > 0U)
    {
        timeout--;
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    if(timeout == 0U)
    {
        USART1_SendString("RTC LSE timeout\r\n");
        return;
    }

    if(rtc_initialized == 0U)
    {
        RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
    }
    RCC_RTCCLKCmd(ENABLE);

    if(RTC_WaitForSynchro() == ERROR)
    {
        USART1_SendString("RTC sync failed\r\n");
        return;
    }

    if(rtc_initialized != 0U)
    {
        rtc_ready = 1U;
        USART1_SendString("RTC ready\r\n");
        return;
    }

    rtc_init.RTC_AsynchPrediv = 127U;
    rtc_init.RTC_SynchPrediv = 255U;
    rtc_init.RTC_HourFormat = RTC_HourFormat_24;
    init_status = RTC_Init(&rtc_init);

    // 仅首次启动写入固定时间，正式考勤前需校时
    RTC_TimeStructure.RTC_Hours = 0x18;
    RTC_TimeStructure.RTC_Minutes = 0x05;
    RTC_TimeStructure.RTC_Seconds = 0x50;
    time_status = RTC_SetTime(RTC_Format_BCD, &RTC_TimeStructure);

    RTC_DateStructure.RTC_Date = 0x22;
    RTC_DateStructure.RTC_Month = 0x09;
    RTC_DateStructure.RTC_Year = 0x26;
    RTC_DateStructure.RTC_WeekDay = RTC_Weekday_Tuesday;
    date_status = RTC_SetDate(RTC_Format_BCD, &RTC_DateStructure);

    if(init_status == SUCCESS && time_status == SUCCESS && date_status == SUCCESS)
    {
        RTC_WriteBackupRegister(RTC_BKP_DR0, RTC_INIT_FLAG);
        rtc_ready = 1U;
        USART1_SendString("RTC init OK\r\n");
    }
    else
    {
        USART1_SendString("RTC init failed\r\n");
    }
}

// LSE 起振且 RTC 同步成功后返回 1
uint8_t RTC_IsReady(void)
{
    return rtc_ready;
}

// 读取 RTC 并显示到 OLED 顶部
void RTC_ShowTimeDate(void)
{
    char show_time[9];
    char show_date[11];

    if(rtc_ready == 0U)
    {
        OLED_ShowString(34, 0, (uint8_t *)"RTC error", 8);
        return;
    }

    RTC_GetTime(RTC_Format_BIN, &RTC_TimeStructure);
    RTC_GetDate(RTC_Format_BIN, &RTC_DateStructure);

    // OLED 不处理换行符，字符串中不加入 \r\n
    sprintf(show_time, "%02u:%02u:%02u", RTC_TimeStructure.RTC_Hours,
            RTC_TimeStructure.RTC_Minutes, RTC_TimeStructure.RTC_Seconds);
    sprintf(show_date, "20%02u-%02u-%02u", RTC_DateStructure.RTC_Year,
            RTC_DateStructure.RTC_Month, RTC_DateStructure.RTC_Date);

    OLED_ShowString(34, 0, (uint8_t *)show_date, 8);
    OLED_ShowString(40, 1, (uint8_t *)show_time, 8);
}
