#ifndef __USER_INCLUDE_H__
#define __USER_INCLUDE_H__

#include "include.h"
#include "user_config.h"

#define ARRAY_SIEZE(array) (sizeof(array) / sizeof(array[0]))
#define USER_DATA_VALID_VAL 0xC5 // 用户数据有效时，对应的数值

// 需要掉电保存的数据
typedef struct __attribute__((packed))
{
    u8 valid;             // 校验，用于验证是不是第一次上电，之前写入的数据是否有效
    u8 is_scan_en;        // 搜索功能是否使能
    u8 is_ble_addr_valid; // 标志位，用于验证蓝牙地址是否有效，0：蓝牙地址无效（没有记录蓝牙地址），1：蓝牙地址有效
    u8 ble_addr[6];       // 存放从机的蓝牙地址
} user_data_t;

// 定义用户自定义的延时控制结构
typedef struct
{
    u32 trigger_tick; // 通过调用 tick_get() 来更新
    u32 delay_ms;     // 目标延时时间
    // u8 pending;       // 延时是否完成（由于可以借助官方提供的接口检测到任务是否延时到了指定时间 tick_check_expire()，这里不添加这个变量）
    u8 is_enable; // 该任务是否使能
    // u8 is_handle_once; // 该任务是否只执行一次（默认只执行一次，所以不加这个功能）
    void (*callback)(void); // 延时完成时，执行的回调函数
} user_delay_ctx_t;         // control timer x，执行延时操作的控制块

// 需要延时一段时间再执行的任务id
typedef enum
{
    // USER_DELAY_CTX_ID_BLE_SCAN_RE_EN = 0, // 蓝牙搜索重新使能
    USER_DELAY_CTX_ID_BLE_CONNECT_SUCCESS_FEEDBACK,
    USER_DELAY_CTX_ID_MAX,
} user_delay_ctx_id_t;

extern volatile user_data_t user_data;

void user_data_write(void);
void user_data_read(void);

// ===================================================================
void user_delay_ctx_ble_connect_success_feedback_handle(void);

void user_delay_ctx_init(void);
void user_delay_ctx_set(user_delay_ctx_id_t id, u32 delay_ms, int (*callback)(void));
void user_delay_ctx_cancel(user_delay_ctx_id_t id);
void user_delay_ctx_handle(void);
// ===================================================================

void user_init(void);
void user_main(void);

#endif