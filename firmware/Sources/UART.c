/*
 * UART.c
 * UART1通信驱动实现
 * 适用于KL25Z128微控制器（Blazar教学系统）
 * 
 * 通信参数：波特率9600，8位数据位，无奇偶校验，1位停止位
 */

#include "derivative.h"  /* 包含MKL25Z4.h，定义所有寄存器 */

/*
 * 将4位二进制数转换为ASCII字符
 * 参数：num - 0-15之间的数
 * 返回：对应ASCII字符（0-9或A-F）
 * 
 * 问号表达式说明：(条件)? 值1 : 值2
 * 等价于 if-else，但更简洁
 * 如果num>=10，返回'A'+num-10（即A-F）
 * 否则，返回'0'+num（即0-9）
 */
unsigned char dtoa(unsigned char num)
{
    return (num >= 10) ? (num + 'A' - 10) : (num + '0');
}

/*
 * UART1时钟初始化
 * 功能：使能UART1模块时钟和PORTC端口时钟
 */
void UART1_SIM_Init(void)
{
    /* SIM_SCGC4: 使能UART1时钟 */
    SIM_SCGC4 |= SIM_SCGC4_UART1_MASK;
    
    /* SIM_SCGC5: 使能PORTC端口时钟（UART1引脚在PTC3/4） */
    SIM_SCGC5 |= SIM_SCGC5_PORTC_MASK;
}

/*
 * UART1端口引脚配置
 * 功能：将PTC3和PTC4配置为UART1的RX和TX功能
 * MUX=0x3 表示选择第3号功能（UART1）
 */
void UART1_PORT_Init(void)
{
    /* PORTC_PCR3: 配置PTC3为 UART1_RX */
    PORTC_PCR3 = PORT_PCR_MUX(0x3);
    
    /* PORTC_PCR4: 配置PTC4为 UART1_TX */
    PORTC_PCR4 = PORT_PCR_MUX(0x3);
}

/*
 * UART1波特率配置
 * 参数：
 *   sysclk - 系统时钟频率（单位：kHz）
 *   baud   - 目标波特率（如9600）
 * 
 * 波特率计算：SBR = sysclk / (16 × baud)
 */
void UART1_Config(unsigned int sysclk, unsigned int baud)
{
    unsigned short sbr;
    
    /* 1. 禁用发送器和接收器，在设置期间防止干扰 */
    UART1_C2 &= ~(UART_C2_TE_MASK | UART_C2_RE_MASK);
    
    /* 2. 配置UART格式：8位数据位，无奇偶校验，1位停止位 */
    UART1_C1 = 0x00;
    
    /* 3. 计算波特率分频系数 SBR */
    sbr = (unsigned short)((sysclk * 1000) / (baud * 16));
    
    /* 4. 设置波特率寄存器 */
    /* BDH: 高5位存放SBR的高5位（掩码0x1F = 00011111b）*/
    UART1_BDH = (unsigned char)((sbr >> 8) & 0x1F);
    /* BDL: 低8位存放SBR的低8位 */
    UART1_BDL = (unsigned char)(sbr & 0xFF);
    
    /* 5. 使能接收器和发送器，以及接收中断 */
    UART1_C2 |= UART_C2_TE_MASK | UART_C2_RE_MASK | UART_C2_RIE_MASK;
}

/*
 * UART1初始化主函数
 * 使用系统时钟10.5MHz，波特率9600
 */
void UART1_Init(void)
{
    UART1_SIM_Init();           /* 初始化时钟 */
    UART1_PORT_Init();          /* 配置引脚 */
    UART1_Config(10500, 9600);  /* 配置波特率 */
}

/*
 * UART1发送单个字符（阻塞式）
 * 等待发送缓冲区为空后才发送
 * 
 * UART1_S1: 状态寄存器1
 * Bit 7 (0x80): TDRE - 发送数据寄存器空标志
 * 当TDRE=1时，表示可以发送新数据
 */
void UART1_PutChar(unsigned char data)
{
    while(!(UART1_S1 & 0x80));  /* 等待发送缓冲区空 */
    UART1_D = data;              /* 写入数据寄存器 */
}

/*
 * UART1接收单个字符（阻塞式）
 * 返回：接收到的字符
 * 
 * UART1_S1: 状态寄存器1
 * Bit 5 (0x20): RDRF - 接收数据就绪标志
 * 当RDRF=1时，表示已接收到一个完整字节
 */
unsigned char UART1_GetChar(void)
{
    while(!(UART1_S1 & 0x20));  /* 等待接收到数据 */
    return UART1_D;              /* 读取数据寄存器 */
}

/*
 * UART1发送字符串
 * 参数：pt - 指向字符串的指针
 * 
 * C语言约定：字符串以'\0'（ASCII 0）作为结束标志
 * 循环直到遇到结束符为止
 */
void UART1_SendString(char *pt)
{
    while((*pt) != 0)           /* 循环直到字符串结束 */
    {
        UART1_PutChar(*pt);     /* 发送当前字符 */
        pt++;                   /* 指针指向下一个字符 */
    }
}

/*
 * UART1发送十六进制数
 * 参数：num - 要发送的字节（0x00-0xFF）
 * 功能：发送形如"0xFF"的十六进制表示
 * 
 * 原理：
 * 1. 发送"0x"前缀
 * 2. 提取高4位，转换为ASCII发送
 * 3. 提取低4位，转换为ASCII发送
 */
void UART1_SendHex(unsigned char num)
{
    /* 发送"0x"前缀 */
    UART1_PutChar('0');
    UART1_PutChar('x');
    
    /* 发送高4位：先与0xF0做与运算取高4位，再右移4位 */
    UART1_PutChar(dtoa((num & 0xF0) >> 4));
    
    /* 发送低4位：与0x0F做与运算取低4位 */
    UART1_PutChar(dtoa(num & 0x0F));
}
