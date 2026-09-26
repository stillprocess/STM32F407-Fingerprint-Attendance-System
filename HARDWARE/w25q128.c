#include "w25q128.h"
#include "spi1.h"
#include "stm32f4xx.h"
#include <stddef.h>


#define W25Q_CS_HIGH() GPIO_SetBits(GPIOB, GPIO_Pin_14)
#define W25Q_CS_LOW()  GPIO_ResetBits(GPIOB, GPIO_Pin_14)

static uint8_t SPI_ReadWriteByte(uint8_t data);
static void W25Q128_WriteEnable(void);
static void W25Q128_SectorErase(uint32_t addr);
static void W25Q128_WaitBusy(void);
static void W25Q128_PageProgram(uint32_t addr, const uint8_t *data, uint16_t len);

// 初始化 W25Q128 使用的 SPI1
void W25Q128_Config(void)
{
	
     SPI_Config();
	
}

// SPI1 收发一个字节
static uint8_t SPI_ReadWriteByte(uint8_t data)
{
    while (SPI_GetFlagStatus(SPI1, SPI_FLAG_TXE) == RESET);

    SPI_SendData(SPI1, data);

    while (SPI_GetFlagStatus(SPI1, SPI_FLAG_RXNE) == RESET);

    return (uint8_t)SPI_ReceiveData(SPI1);
}

// 发送 Flash 写使能指令
static void W25Q128_WriteEnable(void)
{
    W25Q_CS_LOW();

    SPI_ReadWriteByte(0x06);

    W25Q_CS_HIGH();
}
	
// 擦除地址所在的 4 KB 扇区
static void W25Q128_SectorErase(uint32_t addr)
{
	W25Q128_WriteEnable();
	W25Q_CS_LOW();
	SPI_ReadWriteByte(0x20);
	SPI_ReadWriteByte((addr & 0xFF0000) >> 16);
	SPI_ReadWriteByte((addr & 0xFF00) >> 8);
	SPI_ReadWriteByte(addr & 0xFF);
	W25Q_CS_HIGH();
}

// 等待 Flash 完成擦除或写入
static void W25Q128_WaitBusy(void)
{
    W25Q_CS_LOW();
    SPI_ReadWriteByte(0x05);
    while(SPI_ReadWriteByte(0x00) & 0x01);
    W25Q_CS_HIGH();
}

// 在单个 256 字节页内写入数据
static void W25Q128_PageProgram(uint32_t addr, const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if(data == NULL || len == 0U || len > 256U)
    {
        return;
    }

    // 页编程不能跨越 256 字节边界
    if(((addr & 0xFFU) + len) > 256U)
    {
        return;
    }

    W25Q128_WriteEnable();

    W25Q_CS_LOW();

    SPI_ReadWriteByte(0x02);

    SPI_ReadWriteByte((addr >> 16) & 0xFF);
    SPI_ReadWriteByte((addr >> 8) & 0xFF);
    SPI_ReadWriteByte(addr & 0xFF);

    for (i = 0; i < len; i++)
    {
        SPI_ReadWriteByte(data[i]);
    }

    W25Q_CS_HIGH();

    W25Q128_WaitBusy();
}

// 擦除密码扇区并保存密码摘要
void Password_Save(uint32_t addr, uint8_t *data, uint16_t len)
{
    // 密码哈希独占一个扇区，写入前先擦除
    W25Q128_SectorErase(addr);
    W25Q128_WaitBusy();
    W25Q128_PageProgram(addr, data, len);
}

// 从指定地址读取数据
void W25Q128_Read(uint32_t addr, uint8_t *data, uint16_t len)
{
	uint16_t i;

    if (data == NULL || len == 0)
    {
        return;
    }

    W25Q_CS_LOW();

    SPI_ReadWriteByte(0x03);

    SPI_ReadWriteByte((addr >> 16) & 0xFF);
    SPI_ReadWriteByte((addr >> 8) & 0xFF);
    SPI_ReadWriteByte(addr & 0xFF);

    for (i = 0; i < len; i++)
    {
        data[i] = SPI_ReadWriteByte(0x00);
    }

    W25Q_CS_HIGH();
}

// 在独立扇区中追加一条考勤记录
int32_t Attendance_SaveRecord(const AttendanceRecord_t *record)
{
    uint16_t index;
    uint16_t magic;
    uint32_t address;

    if(record == NULL)
    {
        return -1;
    }

    for(index = 0; index < ATTENDANCE_MAX_RECORDS; index++)
    {
        address = ATTENDANCE_BASE_ADDR + (uint32_t)index * ATTENDANCE_RECORD_SIZE;
        W25Q128_Read(address, (uint8_t *)&magic, sizeof(magic));

        // 擦除态为 0xFFFF，第一个空槽用于追加记录
        if(magic == 0xFFFFU)
        {
            W25Q128_PageProgram(address, (const uint8_t *)record, sizeof(AttendanceRecord_t));
            return (int32_t)index;
        }
    }

    return -1;
}

// 读取指定位置的考勤记录
int32_t Attendance_ReadRecord(uint16_t index, AttendanceRecord_t *record)
{
    uint32_t address;

    if(record == NULL || index >= ATTENDANCE_MAX_RECORDS)
    {
        return -1;
    }

    address = ATTENDANCE_BASE_ADDR + (uint32_t)index * ATTENDANCE_RECORD_SIZE;
    W25Q128_Read(address, (uint8_t *)record, sizeof(AttendanceRecord_t));

    if(record->magic != ATTENDANCE_RECORD_MAGIC)
    {
        return -1;
    }

    return 0;
}

// 擦除全部考勤记录
int32_t Attendance_ClearRecords(void)
{
    uint16_t magic;

    // 100 条记录位于同一个 4 KB 扇区
    W25Q128_SectorErase(ATTENDANCE_BASE_ADDR);
    W25Q128_WaitBusy();

    W25Q128_Read(ATTENDANCE_BASE_ADDR, (uint8_t *)&magic, sizeof(magic));
    return (magic == 0xFFFFU) ? 0 : -1;
}
