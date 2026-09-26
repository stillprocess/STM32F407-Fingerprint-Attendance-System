#include "key.h"
#include "stm32f4xx.h"

// 配置一个独立按键的 NVIC 通道
static void Key_EnableIRQ(IRQn_Type irq)
{
    NVIC_InitTypeDef nvic_init;

    nvic_init.NVIC_IRQChannel = irq;
    // ISR 调用 FromISR API，中断优先级不能高于 FreeRTOS 限制
    nvic_init.NVIC_IRQChannelPreemptionPriority = configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY;
    nvic_init.NVIC_IRQChannelSubPriority = 0;
    nvic_init.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic_init);
}

// 初始化 S1～S4 输入和下降沿中断
void key_init(void)
{
    GPIO_InitTypeDef gpio_init;
    EXTI_InitTypeDef exti_init;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOE, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

    gpio_init.GPIO_Pin = GPIO_Pin_0;
    gpio_init.GPIO_Mode = GPIO_Mode_IN;
    gpio_init.GPIO_Speed = GPIO_High_Speed;
    gpio_init.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &gpio_init);

    gpio_init.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
    GPIO_Init(GPIOE, &gpio_init);

    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource0);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOE, EXTI_PinSource2);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOE, EXTI_PinSource3);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOE, EXTI_PinSource4);

    exti_init.EXTI_Line = EXTI_Line0 | EXTI_Line2 | EXTI_Line3 | EXTI_Line4;
    exti_init.EXTI_Mode = EXTI_Mode_Interrupt;
    exti_init.EXTI_Trigger = EXTI_Trigger_Falling;
    exti_init.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti_init);
    EXTI_ClearITPendingBit(EXTI_Line0 | EXTI_Line2 | EXTI_Line3 | EXTI_Line4);

    Key_EnableIRQ(EXTI0_IRQn);
    Key_EnableIRQ(EXTI2_IRQn);
    Key_EnableIRQ(EXTI3_IRQn);
    Key_EnableIRQ(EXTI4_IRQn);
}

// 将 EXTI 状态转换为 FreeRTOS 事件位
static void Key_SendEventFromISR(uint32_t exti_line, EventBits_t event_bit)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    if(EXTI_GetITStatus(exti_line) == SET)
    {
        // ISR 只发送按键事件，消抖和业务处理放到任务中
        xEventGroupSetBitsFromISR(KeyEventGroup, event_bit, &higher_priority_task_woken);
        EXTI_ClearITPendingBit(exti_line);
    }

    // 高优先级任务被唤醒时立即请求任务切换
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

// S1 按键中断
void EXTI0_IRQHandler(void)
{
    Key_SendEventFromISR(EXTI_Line0, EVENT_GROUP_KEY1_DOWN);
}

// S2 按键中断
void EXTI2_IRQHandler(void)
{
    Key_SendEventFromISR(EXTI_Line2, EVENT_GROUP_KEY2_DOWN);
}

// S3 按键中断
void EXTI3_IRQHandler(void)
{
    Key_SendEventFromISR(EXTI_Line3, EVENT_GROUP_KEY3_DOWN);
}

// S4 按键中断
void EXTI4_IRQHandler(void)
{
    Key_SendEventFromISR(EXTI_Line4, EVENT_GROUP_KEY4_DOWN);
}
