#ifndef __STM32F4XX_IT_H__
#define __STM32F4XX_IT_H__

void NMI_Handler(void);        // 不可屏蔽中断
void HardFault_Handler(void);  // 硬件异常
void MemManage_Handler(void);  // 内存管理异常
void BusFault_Handler(void);   // 总线异常
void UsageFault_Handler(void); // 用法异常
void DebugMon_Handler(void);   // 调试监控异常

#endif
