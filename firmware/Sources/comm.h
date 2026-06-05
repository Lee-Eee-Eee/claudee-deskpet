/*
 * comm.h  —  PC <-> 板 串口通信 (UART1, 9600 8N1)
 *  RX: 中断接收 1 字节 CC 状态码 → cc_state
 *  TX: 回传 accept / deny
 */
#ifndef COMM_H_
#define COMM_H_

#include "claudee.h"

void       comm_init(void);        /* UART1 9600 8N1 + RX 中断(IRQ13) */
cc_state_t comm_state(void);       /* 当前 CC 状态；串口静默 >3s 自动判为 CC_SLEEP */
void       comm_send_accept(void); /* 发 'A' */
void       comm_send_deny(void);   /* 发 'D' */

/* 遥测(token/cost/ctx)行：有新内容时拷到 dst(<=cap-1 字符)并返回1，否则0 */
int        comm_get_telemetry(char *dst, int cap);

/* 情境行(挑战时的工具描述 / ASK 的问题)：有新内容时拷到 dst 并返回1，否则0 */
int        comm_get_context(char *dst, int cap);

#endif /* COMM_H_ */
