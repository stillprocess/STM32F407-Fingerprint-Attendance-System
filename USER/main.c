#include "app_fingerprint.h"
#include "app_keyboard.h"
#include "app_oled.h"
#include "app_servo.h"
#include "beep.h"
#include "FreeRTOS.h"
#include "keyboard.h"
#include "oled.h"
#include "RTC.h"
#include "stm32f4xx.h"
#include "task.h"
#include "uart.h"
#include "w25q128.h"

// 初始化失败时打印任务名并停在此处
static void TaskCreateFailed(const char *task_name)
{
    USART1_SendString(task_name);
    USART1_SendString(" create failed\r\n");
    for( ;; );
}

// 业务任务在全部硬件和通信对象就绪后创建
static void AppTasks_Create(void)
{
    if(xTaskCreate(TaskKeyboardUnlock, "Keyboard", 512, NULL, 1, NULL) != pdPASS)
    {
        TaskCreateFailed("Keyboard");
    }

    if(xTaskCreate(TaskOLED_ShowTime, "OLED", 512, NULL, 1, NULL) != pdPASS)
    {
        TaskCreateFailed("OLED");
    }

    if(xTaskCreate(app_task_key, "FPM Key", 512, NULL, 2, NULL) != pdPASS)
    {
        TaskCreateFailed("FPM Key");
    }

    if(xTaskCreate(app_task_fpm, "FPM", 512, NULL, 2, NULL) != pdPASS)
    {
        TaskCreateFailed("FPM");
    }

    if(xTaskCreate(TaskServo, "Servo", 256, NULL, 1, NULL) != pdPASS)
    {
        TaskCreateFailed("Servo");
    }
}

// 初始化外设、互斥锁和队列后创建业务任务
static void TaskAll_Init(void *pvParameters)
{
    (void)pvParameters;

    // 初始化任务优先完成底层资源，避免业务任务访问空队列或未初始化外设
    key_board_init();
    W25Q128_Config();
    RTC_Config();
    Beep_Config();

    if(OLED_AppInit() != pdPASS)
    {
        TaskCreateFailed("OLED mutex");
    }

    OLED_Init();
    OLED_Clear();

    if(ServoApp_Init() != pdPASS)
    {
        TaskCreateFailed("Servo queue");
    }

    if(FingerprintApp_Init() != pdPASS)
    {
        TaskCreateFailed("FPM queue");
    }

    AppTasks_Create();
    USART1_SendString("Init OK\r\n");

    // 初始化只执行一次，完成后释放任务栈
    vTaskDelete(NULL);
}

// 先启动调度器，再由初始化任务配置业务外设
int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    USART1_Config(9600);

    // 调度器启动时只运行初始化任务
    if(xTaskCreate(TaskAll_Init, "Init", 512, NULL, 3, NULL) != pdPASS)
    {
        TaskCreateFailed("Init");
    }

    vTaskStartScheduler();
    for( ;; );
}
