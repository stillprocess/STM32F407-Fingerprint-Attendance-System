#include "uart.h"
#include "stm32f4xx.h"
#include <stddef.h>

volatile uint8_t u3_recvbuf[512] = {0};
volatile uint32_t u3_recvcnt = 0;

// 配置串口通用通信参数
static void USART_InitCommon(USART_TypeDef *usart, uint32_t baud)
{
    USART_InitTypeDef usart_init;

    usart_init.USART_BaudRate = baud;
    usart_init.USART_WordLength = USART_WordLength_8b;
    usart_init.USART_StopBits = USART_StopBits_1;
    usart_init.USART_Parity = USART_Parity_No;
    usart_init.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    usart_init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(usart, &usart_init);
    USART_Cmd(usart, ENABLE);
}

// 开启串口接收中断
static void USART_EnableRxInterrupt(USART_TypeDef *usart, IRQn_Type irq)
{
    NVIC_InitTypeDef nvic_init;

    nvic_init.NVIC_IRQChannel = irq;
    nvic_init.NVIC_IRQChannelPreemptionPriority = 0;
    nvic_init.NVIC_IRQChannelSubPriority = 1;
    nvic_init.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic_init);

    USART_ITConfig(usart, USART_IT_RXNE, ENABLE);
    USART_ClearITPendingBit(usart, USART_IT_RXNE);
}

// 初始化 USART1 调试串口
void USART1_Config(uint32_t baud)
{
    GPIO_InitTypeDef gpio_init;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

    gpio_init.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    gpio_init.GPIO_Mode = GPIO_Mode_AF;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_100MHz;
    gpio_init.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &gpio_init);

    // USART1 当前只用于日志输出，不开启接收中断
    USART_InitCommon(USART1, baud);
}

// USART1 发送字符串
void USART1_SendString(const char *str)
{
    if(str == NULL)
    {
        return;
    }

    while(*str != '\0')
    {
        USART_SendData(USART1, (uint16_t)*str++);
        while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
        {
        }
    }
}

// 初始化 USART2 指纹模块串口
void USART2_Config(uint32_t baud)
{
    GPIO_InitTypeDef gpio_init;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);

    gpio_init.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
    gpio_init.GPIO_Mode = GPIO_Mode_AF;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_100MHz;
    gpio_init.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &gpio_init);

    USART_InitCommon(USART2, baud);
    // USART2 接收中断由 FPM383F 驱动处理
    USART_EnableRxInterrupt(USART2, USART2_IRQn);
}

// 初始化 USART3 蓝牙串口
void USART3_Config(uint32_t baud)
{
    GPIO_InitTypeDef gpio_init;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_USART3);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource11, GPIO_AF_USART3);

    gpio_init.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    gpio_init.GPIO_Mode = GPIO_Mode_AF;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_100MHz;
    gpio_init.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &gpio_init);

    USART_InitCommon(USART3, baud);
    // USART3 保留给蓝牙串口，接收数据存入线性缓冲区
    USART_EnableRxInterrupt(USART3, USART3_IRQn);
}

// USART3 发送字符串
void USART3_SendString(const char *str)
{
    if(str == NULL)
    {
        return;
    }

    while(*str != '\0')
    {
        USART_SendData(USART3, (uint16_t)*str++);
        while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET)
        {
        }
    }
}

// 清空 USART3 接收缓冲区
void USART3_ClearRxBuffer(void)
{
    // 修改共享计数前暂停接收中断
    USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
    u3_recvcnt = 0;
    u3_recvbuf[0] = '\0';
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
}

// 从 USART3 缓冲区移除已处理数据
void USART3_DiscardRxBytes(uint32_t length)
{
    uint32_t index;
    uint32_t remain;

    USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);

    if(length >= u3_recvcnt)
    {
        u3_recvcnt = 0;
        u3_recvbuf[0] = '\0';
    }
    else
    {
        remain = u3_recvcnt - length;
        for(index = 0; index < remain; index++)
        {
            u3_recvbuf[index] = u3_recvbuf[length + index];
        }
        u3_recvcnt = remain;
        u3_recvbuf[remain] = '\0';
    }

    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
}

// 将 USART3 接收字节写入缓冲区
void USART3_IRQHandler(void)
{
    uint8_t data;

    if(USART_GetITStatus(USART3, USART_IT_RXNE) == SET)
    {
        data = (uint8_t)USART_ReceiveData(USART3);
        // 末尾保留 '\0'，便于按字符串处理
        if(u3_recvcnt < sizeof(u3_recvbuf) - 1U)
        {
            u3_recvbuf[u3_recvcnt++] = data;
            u3_recvbuf[u3_recvcnt] = '\0';
        }
    }
}
