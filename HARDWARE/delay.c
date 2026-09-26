#include "delay.h"
#include "stm32f4xx.h"

// 使用 SysTick 当前计数值延时微秒
void delay_us(uint32_t nus)
{
    uint32_t elapsed = 0;
    uint32_t reload = SysTick->LOAD;
    uint32_t previous = SysTick->VAL;
    uint32_t current;
    uint32_t target = nus * (SystemCoreClock / 1000000U);

    // FreeRTOS 占用 SysTick，只读取计数值，不修改寄存器配置
    while(elapsed < target)
    {
        current = SysTick->VAL;
        if(previous != current)
        {
            if(previous > current)
            {
                elapsed += previous - current;
            }
            else
            {
                elapsed += reload + 1U - current + previous;
            }
            previous = current;
        }
    }
}

// 按毫秒循环调用微秒延时
void delay_ms(uint32_t nms)
{
    while(nms-- > 0U)
    {
        delay_us(1000U);
    }
}
