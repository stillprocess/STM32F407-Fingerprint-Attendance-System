#include "FPM383F.h"
#include "FreeRTOS.h"
#include "stm32f4xx.h"
#include "task.h"
#include "uart.h"
#include <stddef.h>


// FPM383F 使用 USART2，波特率 57600
static volatile uint8_t  g_usart2_buf[64];
static volatile uint32_t g_usart2_cnt=0;

// USART2 连续 1 ms 没有新字节时认为一帧接收完成
static volatile uint32_t g_usart2_event=0;

static NVIC_InitTypeDef   NVIC_InitStructure;
static TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

// 指令 0x3C：红、绿、蓝三种 LED 固定帧
static const uint8_t fpm_led_blue[16]  = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x07,0x3C,0x03,0x01,0x01,0x00,0x00,0x49};
static const uint8_t fpm_led_red[16]   = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x07,0x3C,0x02,0x04,0x04,0x02,0x00,0x50};
static const uint8_t fpm_led_green[16] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x07,0x3C,0x02,0x02,0x02,0x02,0x00,0x4C};

// TIM5 每 1 ms 检查一次 USART2 接收计数
static void TIM5_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);

	// 84 MHz / 8400 / 10 = 1 kHz
	TIM_TimeBaseStructure.TIM_Period = 10-1;
	TIM_TimeBaseStructure.TIM_Prescaler = 8400-1;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	
	TIM_TimeBaseInit(TIM5, &TIM_TimeBaseStructure);
	
	TIM_ITConfig(TIM5,TIM_IT_Update,ENABLE);

    // 中断不调用 FreeRTOS API，可使用高优先级
	NVIC_InitStructure.NVIC_IRQChannel = TIM5_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	
	TIM_Cmd(TIM5,ENABLE);
}


// 校验和从包标识开始累加
static uint16_t check_sum(uint8_t *buf,uint32_t len)
{
	uint16_t sum=0;
	
	uint8_t *p=buf;
	
	while(len--)
	{
		sum+=*p++;
	}
	
	return sum;
}	

// 清空接收状态并发送一帧指令
static void fpm_send_data(int32_t length, const uint8_t data[])
{
	int32_t i;
	
	// 发命令前清除上一帧状态
	g_usart2_event = 0;
	g_usart2_cnt = 0;

	for(i = 0;i<length;i++)
	{
		USART_SendData(USART2, data[i]);
		
		// 等待发送寄存器为空
		while(!USART_GetFlagStatus(USART2, USART_FLAG_TXE))
		{
		}
	}
	
}

// 设置指纹模块灯光颜色
uint8_t fpm_ctrl_led(uint8_t color)
{

	if(color == FPM_LED_RED)
	{
		fpm_send_data(16, fpm_led_red);
	}
	
	if(color == FPM_LED_GREEN)
	{
		fpm_send_data(16, fpm_led_green);
	}
	
	if(color == FPM_LED_BLUE)
	{
		fpm_send_data(16, fpm_led_blue);
	}
	
	// LED 指令也有应答，等待结束后再发送下一条指令
	{
		uint32_t timeout = 200;
		while(!g_usart2_event && (--timeout))
		{
			vTaskDelay(pdMS_TO_TICKS(1));
		}
	}
	return 0;
}

// 发送清空指纹库指令
int32_t fpm_empty(void)
{
    uint32_t timeout=4000;	
	

	uint8_t buf[12] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x03,0x0D,0x00,0x11};	
	
	fpm_send_data(12,buf);
	
	// 等待模块应答
	while(!g_usart2_event && (--timeout))
	{
		vTaskDelay(pdMS_TO_TICKS(1));
	}
	
	if(!timeout)
	{
		USART1_SendString("FPM clear timeout\r\n");
		return -1;
	}

	// 帧头和最小长度错误时不读取确认码
	if(g_usart2_cnt < 10 || g_usart2_buf[0]!=0xEF || g_usart2_buf[1]!=0x01)
	{
		USART1_SendString("FPM clear reply error\r\n");
		return -1;
	}
	
	// 确认码非 0 表示模块拒绝清空
	if(g_usart2_buf[9] != 0x00)
	{

		USART1_SendString("FPM clear rejected\r\n");
		return -1;
		
	}	

	return 0;
	
}

// 按指定 ID 自动录入指纹
int32_t fpm_enroll_auto(uint16_t id)
{
    uint8_t buf[17];
	uint16_t cs=0;
    uint32_t timeout=4000;
	
    // 指令 0x31：自动录入，ID 使用大端格式
    buf[0]=0xEF;buf[1]=0x01;

    buf[2]=0xFF;buf[3]=0xFF;buf[4]=0xFF;buf[5]=0xFF;

    buf[6]=0x01;

    buf[7]=0x00;buf[8]=0x08;

    buf[9]=0x31;

    buf[10]=(id>>8)&0xFF;buf[11]=id&0xFF;

    // 录入一次，模块参数保持 0x003F
    buf[12]=1;
    buf[13]=0x00;
    buf[14]=0x3F;

    cs=check_sum(&buf[6],9);
    
    buf[15]=(cs>>8)&0xFF;
    buf[16]=(cs)&0xFF;   
	
	
	fpm_send_data(17,buf);
	
	// 等待模块应答
	while(!g_usart2_event && (--timeout))
	{
		vTaskDelay(pdMS_TO_TICKS(1));
	}
	
	if(!timeout)
	{
		USART1_SendString("FPM enroll timeout\r\n");
		return -1;
	}

	if(g_usart2_cnt < 10 || g_usart2_buf[0]!=0xEF || g_usart2_buf[1]!=0x01)
	{
		USART1_SendString("FPM enroll reply error\r\n");
		return -1;
	}
	
	// 确认码非 0 表示录入失败或 ID 不可用
	if(g_usart2_buf[9] != 0x00)
	{

		USART1_SendString("FPM enroll rejected\r\n");
		
		return -1;
		
	}	
	

	
	return 0;
}

// 自动验证指纹，并返回匹配到的 ID
int32_t fpm_idenify_auto(uint16_t *id)
{
    uint8_t buf[17];
	uint16_t cs=0;
    uint32_t timeout=4000;
    
    if(id == NULL)
    {
        return -1;
    }

    // 指令 0x32：传入 0xFFFF 时执行 1:N 全库匹配
    buf[0]=0xEF;buf[1]=0x01;

    buf[2]=0xFF;buf[3]=0xFF;buf[4]=0xFF;buf[5]=0xFF;

    buf[6]=0x01;

    buf[7]=0x00;buf[8]=0x08;

    buf[9]=0x32;

    // 最低匹配分数为 80
    buf[10]=80;

    buf[11]=(*id>>8)&0xFF;buf[12]=*id&0xFF;

    buf[13]=0x00;
    buf[14]=0x07;
    
    cs=check_sum(&buf[6],9);
    
    buf[15]=(cs>>8)&0xFF;
    buf[16]=(cs)&0xFF;    
    

	fpm_send_data(17,buf);
	
	// 等待模块应答
	while(!g_usart2_event && (--timeout))
	{
		vTaskDelay(pdMS_TO_TICKS(1));
	}
	
	if(!timeout)
	{
		USART1_SendString("FPM verify timeout\r\n");
		return -1;
	}

	if(g_usart2_cnt < 15 || g_usart2_buf[0]!=0xEF || g_usart2_buf[1]!=0x01)
	{
		USART1_SendString("FPM verify reply error\r\n");
		return -1;
	}
	
	// 确认码非 0 表示未通过模块验证
	if(g_usart2_buf[9] != 0x00)
	{
		USART1_SendString("FPM verify rejected\r\n");
		
		return -1;
	}
	
	// 分数 0xFFFF 表示没有匹配模板
	if(((g_usart2_buf[13]<<8)|g_usart2_buf[14])==0xFFFF)
	{
		USART1_SendString("FPM fingerprint not found\r\n");
		
		return -1;		
	}		
	
	// 匹配成功后返回模板 ID
	*id = (g_usart2_buf[11]<<8)|g_usart2_buf[12];
	
	return 0;
}

// 获取指纹库中的模板数量
int32_t fpm_id_total(uint16_t *total)
{
    uint32_t timeout=4000;	
	
	uint8_t buf[12] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x03,0x1D,0x00,0x21};	
	
	if(total == NULL)
	{
		return -1;
	}

	fpm_send_data(12,buf);
	
	while(!g_usart2_event && (--timeout))
	{
		vTaskDelay(pdMS_TO_TICKS(1));
	}
	
	if(!timeout)
	{
		USART1_SendString("FPM count timeout\r\n");
		return -1;
	}

	if(g_usart2_cnt < 12 || g_usart2_buf[0]!=0xEF || g_usart2_buf[1]!=0x01)
	{
		USART1_SendString("FPM count reply error\r\n");
		return -1;
	}
	
	// 确认码非 0 时数量字段无效
	if(g_usart2_buf[9] != 0x00)
	{
		USART1_SendString("FPM count rejected\r\n");
		return -1;
	}	
	
	// 指纹总数使用大端格式
	*total=(g_usart2_buf[10]<<8)|g_usart2_buf[11];

	return 0;
}

// 从模板索引表查找未使用的 ID
int32_t fpm_find_empty_id(uint16_t *empty_id)
{
    uint8_t buf[13];
    uint32_t timeout=4000;
    uint32_t i;
    uint16_t cs;
    uint16_t received_cs;
    uint16_t id;
    uint8_t index_byte;
    uint8_t index_bit;

    if(empty_id == NULL)
    {
        return -1;
    }

    buf[0]=0xEF;buf[1]=0x01;
    buf[2]=0xFF;buf[3]=0xFF;buf[4]=0xFF;buf[5]=0xFF;
    buf[6]=0x01;
    buf[7]=0x00;buf[8]=0x04;
    // 指令 0x1F 读取模板占用表
    buf[9]=0x1F;
    buf[10]=0x00;
    cs=check_sum(&buf[6],5);
    buf[11]=(cs>>8)&0xFF;
    buf[12]=cs&0xFF;

    fpm_send_data(13,buf);

    while(!g_usart2_event && (--timeout))
    {
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    if(!timeout)
    {
        USART1_SendString("FPM index timeout\r\n");
        return -1;
    }

    if(g_usart2_cnt < 44 || g_usart2_buf[0]!=0xEF || g_usart2_buf[1]!=0x01 || g_usart2_buf[6]!=0x07)
    {
        USART1_SendString("FPM index reply error\r\n");
        return -1;
    }

    if(g_usart2_buf[7]!=0x00 || g_usart2_buf[8]!=0x23 || g_usart2_buf[9]!=0x00)
    {
        USART1_SendString("FPM index rejected\r\n");
        return -1;
    }

    cs=0;
    for(i=6;i<42;i++)
    {
        cs=(uint16_t)(cs+g_usart2_buf[i]);
    }
    received_cs=(uint16_t)(((uint16_t)g_usart2_buf[42]<<8)|g_usart2_buf[43]);
    if(cs!=received_cs)
    {
        USART1_SendString("FPM index checksum error\r\n");
        return -1;
    }

    // ID 0 保留，只在项目允许的 1～50 范围内查找空位
    for(id=1;id<=FPM_MAX_FINGERPRINTS;id++)
    {
        index_byte=(uint8_t)(id/8);
        index_bit=(uint8_t)(id%8);
        if((g_usart2_buf[10+index_byte]&(1U<<index_bit))==0)
        {
            *empty_id=id;
            return 0;
        }
    }

    USART1_SendString("No empty fingerprint ID\r\n");
    return -1;
}

// 初始化指纹串口和接收帧检测定时器
void fpm_init(void)
{
	TIM5_Init();
	USART2_Config(57600);
}

// 接收指纹模块返回的串口字节
void USART2_IRQHandler(void)
{
	static uint8_t d=0;
	
	if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET)
	{
		d=USART_ReceiveData(USART2);
		
		// ISR 只收字节，指纹任务负责解析完整应答
		if(g_usart2_cnt< sizeof(g_usart2_buf))
			g_usart2_buf[g_usart2_cnt++]=d;
		
		USART_ClearITPendingBit(USART2, USART_IT_RXNE);
	}
}

// 检测 USART2 是否已经接收完一帧
void TIM5_IRQHandler(void)
{
	static uint32_t cnt=0;

	if(TIM_GetITStatus(TIM5,TIM_IT_Update) == SET)
	{
		// 计数变化表示应答仍在接收
		if(cnt!=g_usart2_cnt)
		{
			cnt=g_usart2_cnt;
		}
		// 连续 1 ms 无新字节时通知等待中的指纹任务
		else if(cnt && (cnt == g_usart2_cnt))
		{
			g_usart2_event=1;
			cnt=0;
		}
		
		TIM_ClearITPendingBit(TIM5,TIM_IT_Update);
	}
}
