#include "include.h"
#include "user_config.h"
#include "user_include.h"

volatile user_data_t user_data = {0};

static volatile user_delay_ctx_t user_delay_ctx[USER_DELAY_CTX_ID_MAX] = {0};

#define USER_DATE_SAVE_START_ADDR 0x00 // 起始地址

void user_data_write(void)
{
    bsp_param_write(&user_data, USER_DATE_SAVE_START_ADDR, sizeof(user_data_t));
    bsp_param_sync(); // 同步数据到flash
}

void user_data_read(void)
{
    bsp_param_read(&user_data, USER_DATE_SAVE_START_ADDR, sizeof(user_data_t));
    if (user_data.valid != USER_DATA_VALID_VAL)
    {
        // 初始化存放的数据
        user_data.valid = USER_DATA_VALID_VAL;
        user_data.is_ble_addr_valid = 0;
        user_data.is_scan_en = 1; // 默认使能搜索
        memset(user_data.ble_addr, 0, 6);
        user_data_write(); // 将数据写回flash
    }

#if USER_DEBUG_ENABLE
    // 打印读取到的蓝牙地址
    // if (user_data.is_ble_addr_valid != 0)
    // {
    //     for (u8 i = 0; i < 6; i++)
    //     {
    //         my_printf("%02x ", (u16)user_data.ble_addr[i]);
    //     }
    //     my_printf("\n");
    // }
#endif
}

void user_delay_ctx_ble_connect_success_feedback_handle(void)
{
    // 发送连接成功反馈
    uart_send_cmd(CMD_CONNECT_SUCCESS_PREFIX, CMD_CONNECT_SUCCESS_SUFFIX);
}

void user_delay_ctx_init(void)
{
    u8 i;
    for (i = 0; i < ARRAY_SIEZE(user_delay_ctx); i++)
    {
        user_delay_ctx[i].trigger_tick = 0;
        user_delay_ctx[i].delay_ms = 0;
        user_delay_ctx[i].is_enable = 0;
        user_delay_ctx[i].callback = NULL;
    }
}

void user_delay_ctx_set(user_delay_ctx_id_t id, u32 delay_ms, int (*callback)(void))
{
    user_delay_ctx[id].is_enable = 0; // 先屏蔽，最后再使能
    user_delay_ctx[id].trigger_tick = tick_get();
    user_delay_ctx[id].delay_ms = delay_ms;
    user_delay_ctx[id].callback = callback;
    user_delay_ctx[id].is_enable = 1;
}

void user_delay_ctx_cancel(user_delay_ctx_id_t id)
{
    user_delay_ctx[id].is_enable = 0;
}

void user_delay_ctx_handle(void)
{
    u8 i;
    for (i = 0; i < ARRAY_SIEZE(user_delay_ctx); i++)
    {
        if (user_delay_ctx[i].is_enable == 0)
        {
            // 当前轮询到的任务没有使能，继续轮询下一个任务
            continue;
        }

        if (tick_check_expire(user_delay_ctx[i].trigger_tick, user_delay_ctx[i].delay_ms))
        {
            // 延时时间到，执行操作
            // my_printf("user delay ctx %d delayed execution\n", i);
            user_delay_ctx[i].is_enable = 0; // 任务只执行一次

            // 在这里添加需要延时执行操作
            if (user_delay_ctx[i].callback != NULL)
            {
                user_delay_ctx[i].callback();
            }
        }
    }
}

void user_init(void)
{
    user_data_read();
    user_delay_ctx_init();
#if USER_DEBUG_ENABLE
    my_printf("user_init\n");
#endif
}

void user_main(void)
{
    // delay_check();
    user_delay_ctx_handle();
}