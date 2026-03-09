#ifndef _UART_TRANSFER_H
#define _UART_TRANSFER_H


typedef struct {
    u8 timeout;
    u8 done;
    u16 len;
    u8 *buf;
} uart_transfer_cb_typedef;


// 指令码定义
typedef enum
{
    CMD_NONE = 0x00,

    // ==============================================================
    // 传输方向：蓝牙ic -> 客户的单片机
    CMD_CONNECT_BEGIN_PREFIX = 0x92, // 开始连接
    CMD_CONNECT_BEGIN_SUFFIX = 0x98,

    CMD_CONNECT_SUCCEED_PREFIX = 0x93, // 连接成功
    CMD_CONNECT_SUCCEED_SUFFIX = 0x99,

    // CMD_CONNECT_DIS_SUCCEED_PREFIX = 0x95, // 断开连接成功（包括收到断开连接的指令，也包括因掉线断开的情况）
    // CMD_CONNECT_DIS_SUCCEED_SUFFIX = 0x9B,
    // ==============================================================
    // 传输方向： 客户的单片机 -> 蓝牙ic ， 蓝牙ic -> 客户的单片机
    CMD_CONNECT_DIS_PREFIX = 0x94, // 断开连接（主机应该清除记忆的从机的地址）
    CMD_CONNECT_DIS_SUFFIX = 0x9A,
} cmd_code_t;

void uart_send_cmd(cmd_code_t cmd_prefix);

void uart_transfer_init(u32 baud);
void uart_transfer_tx_buff(uint8_t *buff, uint32_t len);
void uart_transfer_rx_event(void);
void uart_timeout_1ms_isr(void);

#endif // _UART_TRANSFER_H
