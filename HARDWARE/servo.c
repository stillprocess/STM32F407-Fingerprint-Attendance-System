#include "servo.h"
#include "stm32f4xx.h"

// 初始化 TIM3 通道 4 舵机 PWM
void Servo_PWM_Init(void)
{
    GPIO_InitTypeDef gpio_init;
    TIM_TimeBaseInitTypeDef timer_init;
    TIM_OCInitTypeDef pwm_init;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    gpio_init.GPIO_Pin = GPIO_Pin_9;
    gpio_init.GPIO_Mode = GPIO_Mode_AF;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_100MHz;
    gpio_init.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOC, &gpio_init);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource9, GPIO_AF_TIM3);

    // TIM3 计数周期为 0.1 ms，PWM 周期为 20 ms
    timer_init.TIM_Prescaler = 8400 - 1;
    timer_init.TIM_CounterMode = TIM_CounterMode_Up;
    timer_init.TIM_Period = 200 - 1;
    timer_init.TIM_ClockDivision = TIM_CKD_DIV1;
    timer_init.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM3, &timer_init);

    pwm_init.TIM_OCMode = TIM_OCMode_PWM1;
    pwm_init.TIM_OutputState = TIM_OutputState_Enable;
    pwm_init.TIM_Pulse = 5;
    pwm_init.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC4Init(TIM3, &pwm_init);

    TIM_OC4PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM3, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
}

// 将角度换算为 PWM 比较值
void Servo_SetAngle(uint16_t angle)
{
    uint16_t pulse;

    if(angle > 180)
    {
        angle = 180;
    }

    // 0～180 度映射到 0.5～2.5 ms 高电平
    pulse = (uint16_t)(5 + ((uint32_t)angle * 20) / 180);
    TIM_SetCompare4(TIM3, pulse);
}
