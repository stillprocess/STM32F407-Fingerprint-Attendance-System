#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

void key_board_init(void); // 初始化矩阵键盘 GPIO
char get_key_board(void);  // 扫描一次矩阵键盘

#endif
