#include "include.h"
#include "driver_gpio.h"
#include "driver_uart.h"
#include "bsp_uart_transfer.h"
#include "ble_user_service.h"

#include "user_config.h"
#include "user_include.h"

#if BSP_UART_TRANSFER_EN

// #define UART_TX_PORT GPIOB_REG
// #define UART_TX_PIN GPIO_PIN_3
// #define UART_RX_PORT GPIOB_REG
// #define UART_RX_PIN GPIO_PIN_4

#define UART_TX_PORT GPIOB_REG
#define UART_TX_PIN GPIO_PIN_4
#define UART_RX_PORT GPIOB_REG
#define UART_RX_PIN GPIO_PIN_3

// 指令帧格式定义
#define CMD_FRAME_HEADER_1 0x80 // 帧头第 1 字节
#define CMD_FRAME_HEADER_2 0x03 // 帧头第 2 字节
#define CMD_FRAME_HEADER_3 0x03 // 帧头第 3 字节
#define CMD_HEADER_LEN 3

#define CMD_FRAME_FIX_LEN (CMD_HEADER_LEN + 2) // 帧长度 (固定长度，帧头 3 + 命令字 1 + 数据 1)



// 解析状态机
typedef enum
{
    PARSER_STATE_IDLE = 0, // 空闲状态
    PARSER_STATE_HEADER_1, // 等待帧头第 1 字节
    PARSER_STATE_HEADER_2, // 等待帧头第 2 字节
    PARSER_STATE_HEADER_3, // 等待帧头第 3 字节
    PARSER_STATE_DATA_1,   //
    PARSER_STATE_DATA_2,   //
    PARSER_STATE_COMPLETE, // 解析完成
} parser_state_t;

// 解析器上下文
typedef struct
{
    parser_state_t state; // 当前状态
    // uint8_t frame_buf[CMD_FRAME_FIX_LEN]; // 帧缓存
    // uint32_t frame_len;                   // 当前帧已接收长度
    // uint32_t expected_len;                // 期望的帧总长度
} uart_parser_t;

static uart_parser_t parser AT(.tbox.voice_enc_buf); // 解析器实例

static volatile uint8_t uart_transfer_timeout AT(.tbox.voice_enc_buf);
static volatile ring_buf_t uart_transfer_ring_buf AT(.tbox.voice_enc_buf);
static volatile uint8_t uart_transfer_buf[512] AT(.tbox.voice_enc_buf);
// static volatile uint8_t uart_transfer_buf_temp[512];

static volatile uint8_t ble_notify_buff[512]; // 存放准备用ble notify发送出去的数据
static volatile uint16_t ble_notify_buff_len; // 待发送出去的数据长度

static volatile uint8_t uart_cmd_buff[CMD_FRAME_FIX_LEN]; // 存放接收到的命令数据
static volatile uint8_t uart_cmd_buff_recved_len = 0;

// 将一个字节的数据写入缓冲区，后续会用 ble notify 发送出去
static void ble_notify_buff_write_byte(uint8_t data)
{
    ble_notify_buff[ble_notify_buff_len++] = data;
}

// 将缓冲区的数据全部通过 ble notify 发送出去
static void ble_notify_buff_send_all(void)
{
    service_notify_event(ble_notify_buff, ble_notify_buff_len);
    ble_notify_buff_len = 0;
}

// 将一个字节的数据写入缓冲区
void uart_cmd_buff_write_byte(uint8_t data)
{
    uart_cmd_buff[uart_cmd_buff_recved_len++] = data;
}

// 读取缓冲区上一次接收的一个字节数据
uint8_t uart_cmd_buff_read_last_byte(void)
{
    return uart_cmd_buff[uart_cmd_buff_recved_len - 1];
}

// 控制命令不正确，调用该函数，将缓冲区的数据转移到 ble_notify_buff
void uart_cmd_buff_error(void)
{
    // memcpy(ble_notify_buff, uart_cmd_buff, uart_cmd_buff_recved_len);
    uint8_t i;
    for (i = 0; i < uart_cmd_buff_recved_len; i++)
    {
        ble_notify_buff_write_byte(uart_cmd_buff[i]);
    }

    uart_cmd_buff_recved_len = 0;
}

void uart_cmd_buff_clear(void)
{
    uart_cmd_buff_recved_len = 0;
}

// 命令执行成功后，将缓冲区的数据通过串口发送，实现反馈
void uart_cmd_success_feedback(void)
{
    u8 i;
    for (i = 0; i < uart_cmd_buff_recved_len; i++)
    {
        while (uart_get_flag(UART, UART_IT_TX) != SET) // 等待数据发送完成
            ;
        uart_send_data(UART, uart_cmd_buff[i]);
    }
}

AT(.com_text.isr)
void uart_isr(void)
{
    if (uart_get_flag(UART, UART_IT_RX) != RESET)
    {
        u8 sbuf = uart_receive_data(UART);
        uart_clear_flag(UART, UART_IT_RX);

        // ring_buf_write(&uart_transfer_ring_buf, &sbuf, 1);  // 此函数不在公共区，不能在中断调用
        uart_transfer_ring_buf.buf[uart_transfer_ring_buf.buf_write++] = sbuf;
        uart_transfer_ring_buf.data_len++;
        if (uart_transfer_ring_buf.buf_write >= uart_transfer_ring_buf.total_len)
            uart_transfer_ring_buf.buf_write = 0;
        uart_transfer_timeout = 6;
    }
}

AT(.com_text.isr)
void uart_timeout_1ms_isr(void)
{
    if (uart_transfer_timeout > 1)
        uart_transfer_timeout--;
}

#if USER_DEBUG_ENABLE
void user_uart_putchar(char ch)
{
    while (uart_get_flag(UART, UART_IT_TX) != SET)
        ; // 等待发送完成
    uart_send_data(UART, (u16)ch);
}
#endif

// 根据传参，发送一帧对应的控制命令
// 传参是传入指令的前缀
void uart_send_cmd(cmd_code_t cmd_prefix)
{
    u8 cmd_suffix = 0; // 存放指令的后缀
    switch (cmd_prefix)
    {
    case CMD_CONNECT_BEGIN_PREFIX:
        cmd_suffix = CMD_CONNECT_BEGIN_SUFFIX;
        break;
    case CMD_CONNECT_SUCCEED_PREFIX:
        cmd_suffix = CMD_CONNECT_SUCCEED_SUFFIX;
        break;
    case CMD_CONNECT_DIS_PREFIX:
        cmd_suffix = CMD_CONNECT_DIS_SUFFIX;
        break;

    default:
#if USER_DEBUG_ENABLE
        // my_printf("uart_send_cmd error\n");
#endif
        return;
        break;
    }

    WDT_CLR();
    while (uart_get_flag(UART, UART_IT_TX) != SET) // 等待数据发送完成
        ;
    uart_send_data(UART, CMD_FRAME_HEADER_1);
    while (uart_get_flag(UART, UART_IT_TX) != SET) // 等待数据发送完成
        ;
    uart_send_data(UART, CMD_FRAME_HEADER_2);
    while (uart_get_flag(UART, UART_IT_TX) != SET) // 等待数据发送完成
        ;
    uart_send_data(UART, CMD_FRAME_HEADER_3);
    while (uart_get_flag(UART, UART_IT_TX) != SET) // 等待数据发送完成
        ;
    uart_send_data(UART, cmd_prefix);
    while (uart_get_flag(UART, UART_IT_TX) != SET) // 等待数据发送完成
        ;
    uart_send_data(UART, cmd_suffix);
    while (uart_get_flag(UART, UART_IT_TX) != SET) // 等待数据发送完成
        ;
}

void uart_transfer_init(u32 baud)
{
    // uart ring buf init
    ring_buf_init(&uart_transfer_ring_buf, uart_transfer_buf, sizeof(uart_transfer_buf));
    uart_transfer_timeout = 0;

    // uart io init
    uart_init_typedef uart_init_struct;
    gpio_init_typedef gpio_init_structure;

    clk_gate0_cmd(CLK_GATE0_UART, CLK_EN);
    clk_uart_clk_set(CLK_UART_CLK_X24M_DIV_CLK);

    // RX
    gpio_init_structure.gpio_pin = UART_RX_PIN;
    gpio_init_structure.gpio_dir = GPIO_DIR_INPUT;
    gpio_init_structure.gpio_fen = GPIO_FEN_PER;
    gpio_init_structure.gpio_fdir = GPIO_FDIR_SELF;
    gpio_init_structure.gpio_mode = GPIO_MODE_DIGITAL;
    gpio_init_structure.gpio_pupd = GPIO_PUPD_PU10K;
    gpio_init(UART_RX_PORT, &gpio_init_structure);

    // TX
    gpio_init_structure.gpio_pin = UART_TX_PIN;
    gpio_init_structure.gpio_dir = GPIO_DIR_OUTPUT;
    gpio_init_structure.gpio_drv = GPIO_DRV_I_6MA;
    gpio_init(UART_TX_PORT, &gpio_init_structure);

    gpio_func_mapping_config(UART_RX_PORT, UART_RX_PIN, GPIO_CROSSBAR_IN_UARTRX);
    gpio_func_mapping_config(UART_TX_PORT, UART_TX_PIN, GPIO_CROSSBAR_OUT_UARTTX);

    uart_init_struct.baud = baud;
    uart_init_struct.mode = UART_SEND_RECV_MODE;
    uart_init_struct.word_len = UART_WORD_LENGTH_8b;
    uart_init_struct.stop_bits = UART_STOP_BIT_1BIT;
    uart_init_struct.one_line_enable = UART_SEPARATE;
    uart_init_struct.baud_fix = UART_BAUD_FIX;
    uart_init(UART, &uart_init_struct);

    uart_pic_config(UART, uart_isr, 0, UART_IT_RX, ENABLE);

    uart_cmd(UART, ENABLE);

#if USER_DEBUG_ENABLE
    // 只在调试时使用，最终需要去掉这个功能
    my_printf_init(user_uart_putchar); // 注册用户自定义的打印函数
    my_printf("user_uart_init\n");
#endif

    uart_send_cmd(CMD_CONNECT_BEGIN_PREFIX);
}

AT(.com_periph.uart.send)
void uart_transfer_tx_buff(uint8_t *buff, uint32_t len)
{
    if (buff == NULL || len == 0)
        return;

    while (len)
    {
        uart_send_data(UART, *buff++);
        while (uart_get_flag(UART, UART_IT_TX) != SET)
            ;
        len--;
    }
}

// uint8_t test_buff[20];
// uint8_t test_time = 0;
// static uint32_t tick_timeout = 50;
// void uart_transfer_test_event(void)
// {
//     static uint32_t ticks = 0;
//     if (tick_check_expire(ticks, tick_timeout) && test_time > 0)
//     {
//         ticks = tick_get();
//         for (uint8_t i = 0; i < 20; i++)
//             test_buff[i] = i;
//         ble_write_to_server(test_buff, 20);
//         test_time--;
//     }
// }

void uart_transfer_rx_event(void)
{
#if 0
    uint8_t *ptr = NULL;
    u32 len = 0;

#ifdef CLIENT
    // uart_transfer_test_event();
#endif

    if (uart_transfer_timeout == 1)
    {
        uart_transfer_timeout = 0;

        len = ring_buf_read(&uart_transfer_ring_buf, uart_transfer_buf_temp, uart_transfer_ring_buf.data_len);
        ptr = uart_transfer_buf_temp;

#ifdef CLIENT
        // 主机接收串口然后通过BLE WRITE到从机
        ble_write_to_server(ptr, len);
#else
        // 从机接收串口然后通过BLE NOTIFY到主机
        service_notify_event(ptr, len);
#endif
    }
#endif

    u8 byte = 0;
    u32 ret = 0;

    if (uart_transfer_timeout == 1)
    {
        uart_transfer_timeout = 0;

        if (uart_transfer_ring_buf.data_len == 0)
        {
            // 环形缓冲区没有数据，直接退出
            return;
        }

        while (1)
        {
            ret = ring_buf_read(&uart_transfer_ring_buf, &byte, 1); // 每次只读取1个字节的数据
            if (ret == 0)
            {
                break; // 退出循环
            }

            // my_printf("%02x ", (u16)byte);
            // my_printf("parser.state = %02d\n", (u16)parser.state);

            if (parser.state == PARSER_STATE_IDLE)
            {
                if (byte == CMD_FRAME_HEADER_1)
                {
                    uart_cmd_buff_write_byte(byte);
                    parser.state = PARSER_STATE_HEADER_1;
                    continue; // 继续循环，处理下一个字节
                }

                ble_notify_buff_write_byte(byte);
                parser.state = PARSER_STATE_IDLE;
            }
            else if (parser.state == PARSER_STATE_HEADER_1)
            {
                if (byte == CMD_FRAME_HEADER_2)
                {
                    uart_cmd_buff_write_byte(byte);
                    parser.state = PARSER_STATE_HEADER_2;
                    continue; // 继续循环，处理下一个字节
                }

                uart_cmd_buff_error();
                ble_notify_buff_write_byte(byte);
                parser.state = PARSER_STATE_IDLE;
            }
            else if (parser.state == PARSER_STATE_HEADER_2)
            {
                if (byte == CMD_FRAME_HEADER_3)
                {
                    uart_cmd_buff_write_byte(byte);
                    parser.state = PARSER_STATE_HEADER_3;
                    continue; // 继续循环，处理下一个字节
                }

                uart_cmd_buff_error();
                ble_notify_buff_write_byte(byte);
                parser.state = PARSER_STATE_IDLE;
            }
            else if (parser.state == PARSER_STATE_HEADER_3)
            {
                if (byte == CMD_CONNECT_DIS_PREFIX)
                {
                    uart_cmd_buff_write_byte(byte);
                    parser.state = PARSER_STATE_DATA_1;
                    continue;
                }

                uart_cmd_buff_error();
                ble_notify_buff_write_byte(byte);
                parser.state = PARSER_STATE_IDLE;
            }
            else if (parser.state == PARSER_STATE_DATA_1)
            {
                if (uart_cmd_buff_read_last_byte() == CMD_CONNECT_DIS_PREFIX &&
                    byte == CMD_CONNECT_DIS_SUFFIX)
                {
                    // my_printf("server_info.conn_handle == %u\n", server_info.conn_handle); 
                    if (server_info.conn_handle != 0)
                    {
                        // 如果已经连接，断开当前连接
                        ble_disconnect(server_info.conn_handle);
                        // my_printf("disconnect\n"); // 改成在事件处理函数中打印断开连接的信息
                    }

                    // 主机应该清除记录从机的蓝牙地址
                    // 主机应该 xx s内都不打开搜索
                    ble_scan_dis();
                    ble_scan_re_en_delay_set();
                    memset(user_data.ble_addr, 0, sizeof(user_data.ble_addr)); // 清空记录的从机的地址
                    user_data.is_ble_addr_valid = 0;
                    user_data_write(); // 将数据写回flash

                    uart_cmd_buff_write_byte(byte);
                    // uart_cmd_success_feedback(); // 由断开连接事件的处理函数发送，不在这里反馈（ble_client.c -> ble_client_event_callback()）
                }
                else
                {
                    uart_cmd_buff_error();
                    ble_notify_buff_write_byte(byte);
#if USER_DEBUG_ENABLE
                    // my_printf("cmd error\n");
#endif
                }

                uart_cmd_buff_clear();
                parser.state = PARSER_STATE_IDLE; // 处理完成指令或者接收指令出错后，都会到这个状态
            }
        }

        // while 循环退出时，如果 ble notify 缓冲区有数据，则发送数据给串口
        if (ble_notify_buff_len > 0)
        {
            ble_notify_buff_send_all();
#if USER_DEBUG_ENABLE
            // my_printf("ble notify\n");
#endif
        }
    }
}
#endif
