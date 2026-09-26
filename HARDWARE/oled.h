#ifndef __OLED_H__
#define __OLED_H__

#include <stdint.h>

#define OLED_CMD   0U
#define OLED_DATA  1U

void OLED_Init(void);                                                     // 初始化 OLED
void OLED_Clear(void);                                                    // 清屏
void OLED_SetPosition(uint8_t x, uint8_t page);                            // 设置显示位置
void OLED_ShowChar(uint8_t x, uint8_t page, uint8_t chr, uint8_t size);    // 显示一个字符
void OLED_ShowString(uint8_t x, uint8_t page, const uint8_t *str, uint8_t size); // 显示字符串
void OLED_ClearPage(uint8_t page);                                        // 清空指定页

#endif
