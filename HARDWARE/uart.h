#ifndef __UART_H__
#define __UART_H__

#include <stdint.h>

extern volatile uint8_t u3_recvbuf[512]; // USART3 接收缓冲区
extern volatile uint32_t u3_recvcnt;     // USART3 已接收字节数

void USART1_Config(uint32_t baud);                 // 初始化调试串口
void USART1_SendString(const char *str);           // 调试串口发送字符串
void USART2_Config(uint32_t baud);                 // 初始化指纹模块串口
void USART3_Config(uint32_t baud);                 // 初始化蓝牙串口
void USART3_SendString(const char *str);           // 蓝牙串口发送字符串
void USART3_ClearRxBuffer(void);                   // 清空蓝牙接收缓冲区
void USART3_DiscardRxBytes(uint32_t length);       // 丢弃已处理的接收数据

#endif
