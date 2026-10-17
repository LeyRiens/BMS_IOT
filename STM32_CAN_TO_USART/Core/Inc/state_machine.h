#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stddef.h>
#include "config.h"

typedef enum {
    ST_WAIT_READY = 0,   // 等待 ESP8266 启动
    ST_AT_CHECK,         // AT 通信测试
    ST_WIFI_MODE,        // 配置 Wi-Fi Station 模式
    ST_WIFI_CONNECT,     // 连接 Wi-Fi
    ST_MQTT_USERCFG,     // 配置 MQTT 用户参数
    ST_MQTT_CONNCFG,     // 配置 MQTT 连接参数
    ST_MQTT_CONNECT,     // 连接 MQTT Broker
    ST_MQTT_VERIFY,      // 查询并验证 MQTT 连接
    ST_CONNECTED,        // MQTT 已连接
    ST_ERROR,            // 错误状态
    ST_COUNT              // 状态计数，用于数组大小
} ESP_State;

typedef enum {
    EV_READY = 0,           // ESP8266 启动完成
    EV_OK,                  // AT 指令执行成功
    EV_ERROR,               // AT 指令执行失败
    EV_BUSY,                // ESP8266 正忙

    EV_WIFI_CONNECTED,      // Wi-Fi 已连接
    EV_WIFI_GOT_IP,         // 已获取 IP
    EV_WIFI_DISCONNECTED,   // Wi-Fi 断开
    EV_WIFI_ERROR,          // Wi-Fi 连接错误

    EV_MQTT_CONNECTED,      // MQTT 已连接通知
    EV_MQTT_DISCONNECTED,   // MQTT 断开通知
    EV_MQTT_QUERY,          // 收到 MQTT 状态查询结果

    EV_TIMEOUT,             // 超时事件
    EV_COUNT                 // 事件计数，用于数组大小
} ESP_Event;


typedef void (*funcPtr)(void);


static const funcPtr
connect_table[ST_COUNT][EV_COUNT] = {

    [ST_WAIT_READY] = {
        [EV_READY]   = NULL,
        [EV_TIMEOUT] = NULL,
    },

    [ST_AT_CHECK] = {
        [EV_OK]      = NULL,
        [EV_ERROR]   = NULL,    
        [EV_BUSY]    = NULL,
        [EV_TIMEOUT] = NULL,
    },

    [ST_WIFI_MODE] = {
        [EV_OK]      = NULL,
        [EV_ERROR]   = NULL,
        [EV_BUSY]    = NULL,
        [EV_TIMEOUT] = NULL
    },

    [ST_WIFI_CONNECT] = {
        [EV_OK]                = NULL,
        [EV_ERROR]             = NULL,
        [EV_BUSY]              = NULL,
        [EV_TIMEOUT]           = NULL,
        [EV_WIFI_CONNECTED]    = NULL,
        [EV_WIFI_GOT_IP]       = NULL,
        [EV_WIFI_DISCONNECTED] = NULL,
        [EV_WIFI_ERROR]        = NULL
    },

    [ST_MQTT_USERCFG] = {
        [EV_OK]                = NULL,
        [EV_ERROR]             = NULL,
        [EV_BUSY]              = NULL,
        [EV_TIMEOUT]           = NULL,
        [EV_WIFI_DISCONNECTED] = NULL
    },

    [ST_MQTT_CONNCFG] = {
        [EV_OK]                = NULL,
        [EV_ERROR]             = NULL,
        [EV_BUSY]              = NULL,
        [EV_TIMEOUT]           = NULL,
        [EV_WIFI_DISCONNECTED] = NULL
    },

    [ST_MQTT_CONNECT] = {
        [EV_OK]                = NULL,
        [EV_ERROR]             = NULL,
        [EV_BUSY]              = NULL,
        [EV_TIMEOUT]           = NULL,
        [EV_WIFI_DISCONNECTED] = NULL,
        [EV_MQTT_CONNECTED]    = NULL,
        [EV_MQTT_DISCONNECTED] = NULL
    },

    [ST_MQTT_VERIFY] = {
        [EV_OK]                = NULL,
        [EV_ERROR]             = NULL,
        [EV_BUSY]              = NULL,
        [EV_TIMEOUT]           = NULL,
        [EV_WIFI_DISCONNECTED] = NULL,
        [EV_MQTT_DISCONNECTED] = NULL,
        [EV_MQTT_QUERY]        = NULL
    },

    [ST_CONNECTED] = {
        [EV_WIFI_DISCONNECTED] = NULL,
        [EV_MQTT_DISCONNECTED] = NULL
    },

    [ST_ERROR] = {
        [EV_READY] = NULL
    }
};


typedef struct {
    ESP_State current_state;
    ESP_Event current_event;
} Connect_SM_t;

void Connect_SM_Init(Connect_SM_t *sm);
void Connect_SM_Start(Connect_SM_t *sm);




#endif /* STATE_MACHINE_H */