#ifndef __W25Q128_H__
#define __W25Q128_H__

#include <stdint.h>

#define ATTENDANCE_BASE_ADDR       0x001000U // 考勤记录扇区首地址
#define ATTENDANCE_MAX_RECORDS     100U      // 最大记录数量
#define ATTENDANCE_RECORD_SIZE     16U       // 单条记录占用字节数
#define ATTENDANCE_RECORD_MAGIC    0xA55AU   // 有效记录标志

typedef __packed struct
{
    uint16_t magic;       // 有效记录标志
    uint16_t finger_id;   // 指纹 ID
    uint8_t year;         // 年，范围 0～99
    uint8_t month;        // 月
    uint8_t day;          // 日
    uint8_t hour;         // 时
    uint8_t minute;       // 分
    uint8_t second;       // 秒
    uint8_t reserved[6];  // 保留空间
} AttendanceRecord_t;

void W25Q128_Config(void);                                            // 初始化 Flash
void Password_Save(uint32_t addr, uint8_t *data, uint16_t len);       // 保存密码摘要
void W25Q128_Read(uint32_t addr, uint8_t *data, uint16_t len);        // 读取 Flash 数据
int32_t Attendance_SaveRecord(const AttendanceRecord_t *record);      // 追加考勤记录
int32_t Attendance_ReadRecord(uint16_t index, AttendanceRecord_t *record); // 读取考勤记录
int32_t Attendance_ClearRecords(void);                                // 清空考勤记录

#endif
