/*
 * UART.h
 * UART通信驱动头文件
 * 适用于KL25Z128微控制器（Blazar教学系统）
 */

#ifndef UART_H_
#define UART_H_

/* UART初始化函数 */
void UART1_Init(void);

/* 发送单个字符 */
void UART1_PutChar(unsigned char data);

/* 发送字符串（以'\0'结尾） */
void UART1_SendString(char *pt);

/* 发送十六进制数（0x00-0xFF） */
void UART1_SendHex(unsigned char num);

/* 接收单个字符（阻塞式） */
unsigned char UART1_GetChar(void);

/* 将4位二进制数转换为ASCII字符（0-9, A-F） */
unsigned char dtoa(unsigned char num);

#endif /* UART_H_ */
