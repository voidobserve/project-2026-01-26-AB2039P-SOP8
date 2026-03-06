#include "include.h"
#include "user_config.h"
#include "user_include.h"

volatile user_data_t user_data = {0};

static volatile user_delay_ctx_t ble_scan_re_en_ctx; // ble scan 重新使能的控制变量

#define USER_DATE_SAVE_START_ADDR 0x00 // 起始地址

void user_data_write(void)
{
    bsp_param_write(&user_data, USER_DATE_SAVE_START_ADDR, sizeof(user_data_t));
    bsp_param_sync(); // 同步数据到flash
}

void user_data_read(void)
{
    bsp_param_read(&user_data, USER_DATE_SAVE_START_ADDR, sizeof(user_data_t));
#if USER_DEBUG_ENABLE
    // 打印读取到的蓝牙地址
#endif
}

void ble_scan_re_en_delay_set(void)
{
    // 设置延时执行标记
    ble_scan_re_en_ctx.trigger_tick = tick_get();
    ble_scan_re_en_ctx.delay_ms = 50000; // 延时 5000 ms
    ble_scan_re_en_ctx.pending = true;
}

void delay_check(void)
{
    if (ble_scan_re_en_ctx.pending)
    {
        if (tick_check_expire(ble_scan_re_en_ctx.trigger_tick, ble_scan_re_en_ctx.delay_ms))
        {
            // 延时时间到，执行操作
            my_printf("ble scan re-enable delayed execution\n");

            // TODO: 在这里添加你需要延时执行的操作
            ble_scan_en();

            ble_scan_re_en_ctx.pending = false;
        }
    }
}

void user_main(void)
{
    delay_check();
}