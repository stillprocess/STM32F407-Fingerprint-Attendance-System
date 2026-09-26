#include "stm32f4xx_it.h"
#include "stm32f4xx.h"

// SVC、PendSV、SysTick 由 FreeRTOS 端口文件提供

// 异常时直接轮询 USART1，不使用 FreeRTOS 接口
static void Fault_Send(const char *text)
{
    uint32_t timeout;

    // 异常上下文不使用任务、队列或互斥锁
    if((USART1->CR1 & USART_CR1_UE) == 0U)
    {
        return;
    }

    while(*text != '\0')
    {
        timeout = 100000U;
        while((USART1->SR & USART_SR_TXE) == 0U && timeout > 0U)
        {
            timeout--;
        }

        if(timeout == 0U)
        {
            break;
        }

        USART1->DR = (uint8_t)*text++;
    }
}

// 未使用 NMI
void NMI_Handler(void)
{
}

// 硬件异常时打印类型并停机
void HardFault_Handler(void)
{
    Fault_Send("FAULT: HARDFAULT\r\n");
    for( ;; );
}

// 内存管理异常时停机
void MemManage_Handler(void)
{
    Fault_Send("FAULT: MEMFAULT\r\n");
    for( ;; );
}

// 总线异常时停机
void BusFault_Handler(void)
{
    Fault_Send("FAULT: BUSFAULT\r\n");
    for( ;; );
}

// 指令或运算异常时停机
void UsageFault_Handler(void)
{
    Fault_Send("FAULT: USAGEFAULT\r\n");
    for( ;; );
}

// 未使用调试监控异常
void DebugMon_Handler(void)
{
}
