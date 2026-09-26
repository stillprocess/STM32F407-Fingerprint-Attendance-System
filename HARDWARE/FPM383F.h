#ifndef __FPM383F_H__
#define __FPM383F_H__

#include <stdint.h>

#define FPM_LED_RED               1U   // 操作失败
#define FPM_LED_GREEN             2U   // 操作成功
#define FPM_LED_BLUE              3U   // 操作进行中
#define FPM_MAX_FINGERPRINTS      50U  // 本项目允许的最大指纹数量

void fpm_init(void);                              // 初始化指纹模块通信
int32_t fpm_empty(void);                          // 清空全部指纹
int32_t fpm_id_total(uint16_t *total);             // 获取已录入指纹数量
int32_t fpm_find_empty_id(uint16_t *empty_id);     // 查找空闲指纹 ID
uint8_t fpm_ctrl_led(uint8_t color);               // 设置指纹模块灯光
int32_t fpm_idenify_auto(uint16_t *id);            // 自动验证指纹
int32_t fpm_enroll_auto(uint16_t id);              // 自动录入指纹

#endif
