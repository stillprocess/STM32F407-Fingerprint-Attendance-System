#include "keyboard.h"
#include "FreeRTOS.h"
#include "stm32f4xx.h"
#include "task.h"

// 行线：PD6、PD7、PC6、PC8；列线：PC11、PE5、PA6、PC7
// 设置四条矩阵键盘行线电平
static void Keyboard_SetRows(BitAction row1, BitAction row2, BitAction row3, BitAction row4)
{
    GPIO_WriteBit(GPIOD, GPIO_Pin_6, row1);
    GPIO_WriteBit(GPIOD, GPIO_Pin_7, row2);
    GPIO_WriteBit(GPIOC, GPIO_Pin_6, row3);
    GPIO_WriteBit(GPIOC, GPIO_Pin_8, row4);
}

// 读取当前行对应的四个按键
static char Keyboard_ReadColumns(char key1, char key2, char key3, char key4)
{
    if(GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_11) == Bit_RESET)
    {
        return key1;
    }
    if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_5) == Bit_RESET)
    {
        return key2;
    }
    if(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6) == Bit_RESET)
    {
        return key3;
    }
    if(GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_7) == Bit_RESET)
    {
        return key4;
    }

    return 'N';
}

// 初始化矩阵键盘行线和列线
void key_board_init(void)
{
    GPIO_InitTypeDef gpio_init;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOC |
                           RCC_AHB1Periph_GPIOD | RCC_AHB1Periph_GPIOE, ENABLE);

    gpio_init.GPIO_Mode = GPIO_Mode_OUT;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_Speed = GPIO_High_Speed;
    gpio_init.GPIO_PuPd = GPIO_PuPd_NOPULL;

    gpio_init.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_Init(GPIOD, &gpio_init);

    gpio_init.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_8;
    GPIO_Init(GPIOC, &gpio_init);
    // 空闲时所有行保持高电平
    Keyboard_SetRows(Bit_SET, Bit_SET, Bit_SET, Bit_SET);

    gpio_init.GPIO_Mode = GPIO_Mode_IN;
    gpio_init.GPIO_PuPd = GPIO_PuPd_UP;

    gpio_init.GPIO_Pin = GPIO_Pin_6;
    GPIO_Init(GPIOA, &gpio_init);

    gpio_init.GPIO_Pin = GPIO_Pin_5;
    GPIO_Init(GPIOE, &gpio_init);

    gpio_init.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_11;
    GPIO_Init(GPIOC, &gpio_init);
}

// 逐行扫描矩阵键盘，无按键时返回 N
char get_key_board(void)
{
    char key;

    // 每次只拉低一行，读取四个上拉输入列
    Keyboard_SetRows(Bit_RESET, Bit_SET, Bit_SET, Bit_SET);
    vTaskDelay(pdMS_TO_TICKS(2));
    key = Keyboard_ReadColumns('1', '2', '3', 'A');
    if(key != 'N')
    {
        Keyboard_SetRows(Bit_SET, Bit_SET, Bit_SET, Bit_SET);
        return key;
    }

    Keyboard_SetRows(Bit_SET, Bit_RESET, Bit_SET, Bit_SET);
    vTaskDelay(pdMS_TO_TICKS(2));
    key = Keyboard_ReadColumns('4', '5', '6', 'B');
    if(key != 'N')
    {
        Keyboard_SetRows(Bit_SET, Bit_SET, Bit_SET, Bit_SET);
        return key;
    }

    Keyboard_SetRows(Bit_SET, Bit_SET, Bit_RESET, Bit_SET);
    vTaskDelay(pdMS_TO_TICKS(2));
    key = Keyboard_ReadColumns('7', '8', '9', 'C');
    if(key != 'N')
    {
        Keyboard_SetRows(Bit_SET, Bit_SET, Bit_SET, Bit_SET);
        return key;
    }

    Keyboard_SetRows(Bit_SET, Bit_SET, Bit_SET, Bit_RESET);
    vTaskDelay(pdMS_TO_TICKS(2));
    key = Keyboard_ReadColumns('*', '0', '#', 'D');
    Keyboard_SetRows(Bit_SET, Bit_SET, Bit_SET, Bit_SET);

    return key;
}
