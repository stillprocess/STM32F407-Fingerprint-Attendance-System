#include "beep.h"
#include "stm32f4xx.h"

// 初始化 PF8 蜂鸣器控制引脚
void Beep_Config(void)
{
    GPIO_InitTypeDef gpio_init;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);

    gpio_init.GPIO_Pin = GPIO_Pin_8;
    gpio_init.GPIO_Mode = GPIO_Mode_OUT;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_100MHz;
    gpio_init.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOF, &gpio_init);

    // PF8 高电平鸣响，初始化后默认关闭
    beep_OFF();
}

// 输出高电平，打开蜂鸣器
void beep_ON(void)
{
    GPIO_SetBits(GPIOF, GPIO_Pin_8);
}

// 输出低电平，关闭蜂鸣器
void beep_OFF(void)
{
    GPIO_ResetBits(GPIOF, GPIO_Pin_8);
}
