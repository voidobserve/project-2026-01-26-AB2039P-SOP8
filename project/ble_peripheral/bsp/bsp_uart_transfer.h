#ifndef _UART_TRANSFER_H
#define _UART_TRANSFER_H

typedef struct
{
    u8 timeout;
    u8 done;
    u16 len;
    u8 *buf;
} uart_transfer_cb_typedef;

// 定义发送的指令帧
#define CMD_SEND_FRAME_HEADER_1 0x80
#define CMD_SEND_FRAME_HEADER_2 0x03
#define CMD_SEND_FRAME_HEADER_3 0x03
#define CMD_SEND_FRAME_LEN 3
#define CMD_SEND_FRAME_FIX_LEN (CMD_SEND_FRAME_LEN + 2) // 帧长度 == 帧头 + 2字节指令（前缀+后缀）

// 定义接收的指令帧
#define CMD_RECV_FRAME_HEADER_1 0x80
#define CMD_RECV_FRAME_HEADER_2 0x03
#define CMD_RECV_FRAME_HEADER_3 0x02
#define CMD_RECV_FRAME_LEN 3
#define CMD_RECV_FRAME_FIX_LEN (CMD_RECV_FRAME_LEN + 2) // 帧长度 == 帧头 + 2字节指令（前缀+后缀）

// 指令码定义
typedef enum
{
    CMD_NONE = 0x00,

    // ==============================================================
    // 传输方向：蓝牙ic -> 客户的单片机
    // CMD_CONNECT_BEGIN_PREFIX = 0x92, // 开始连接
    // CMD_CONNECT_BEGIN_SUFFIX = 0x98,

    // CMD_CONNECT_SUCCEED_PREFIX = 0x93, // 连接成功
    // CMD_CONNECT_SUCCEED_SUFFIX = 0x99,
    // // ==============================================================
    // // 传输方向： 客户的单片机 -> 蓝牙ic ， 蓝牙ic -> 客户的单片机
    // CMD_CONNECT_DIS_PREFIX = 0x94, // 断开连接（主机应该清除记忆的从机的地址）
    // CMD_CONNECT_DIS_SUFFIX = 0x9A,

    // 传输方向： 语音ic -> 蓝牙ic（主机）
    CMD_START_PAIRING_PREFIX = 0x89, // 开始配对
    CMD_START_PAIRING_SUFFIX = 0x8E,

    CMD_CANCEL_PAIRING_PREFIX = 0x90, // 取消配对
    CMD_CANCEL_PAIRING_SUFFIX = 0x8F,

} cmd_code_t;

void uart_send_cmd(cmd_code_t cmd_prefix, cmd_code_t cmd_suffix);

void uart_transfer_init(u32 baud);
void uart_transfer_tx_buff(uint8_t *buff, uint32_t len);
void uart_transfer_rx_event(void);
void uart_timeout_1ms_isr(void);

#endif // _UART_TRANSFER_H
