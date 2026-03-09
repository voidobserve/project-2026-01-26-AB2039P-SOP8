#ifndef __USER_INCLUDE_H__
#define __USER_INCLUDE_H__

#include "include.h"

#define USER_DATA_VALID_VAL 0xC5 // 用户数据有效时，对应的数值

// 需要掉电保存的数据
typedef struct __attribute__((packed))
{
    u8 valid; // 校验，用于验证是不是第一次上电，之前写入的数据是否有效
    u8 is_ble_addr_valid; // 标志位，用于验证蓝牙地址是否有效，0：蓝牙地址无效（没有记录蓝牙地址），1：蓝牙地址有效
    u8 ble_addr[6];
} user_data_t;

// 定义用户自定义的延时控制结构
typedef struct
{
    u32 trigger_tick;
    u32 delay_ms;
    bool pending;
} user_delay_ctx_t; // control timer x

extern volatile user_data_t user_data;

void user_data_write(void);
void user_data_read(void);

void ble_scan_re_en_delay_set(void);

void user_init(void);
void user_main(void);

#endif