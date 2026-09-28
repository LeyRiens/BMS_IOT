#include "state_user.h"
#include "buffer.h"
#include "config.h"
#include "led.h"
#include "mqtt_at.h"

typedef HAL_StatusTypeDef (*Command_t)(void);
typedef enum { STARTUP, ONLINE, WAIT_PROMPT, SENDING, WAIT_RESULT, CONFIG_REQUIRED } State_t;
static const Command_t s_commands[] = {
    MQTTAT_Test, MQTTAT_SetWifiModeStation, MQTTAT_JoinWifi,
    MQTTAT_SetMqttUserConfig, MQTTAT_SetMqttConnectionConfig, MQTTAT_ConnectMqtt
};
uint32_t g_state_user_error_count;
static State_t s_state;
static uint8_t s_command_index;
static uint32_t s_last_generation;
static uint32_t s_retry_at;

static void Restart(void)
{
    if (MQTTAT_Abort() != HAL_OK) Error_Handler();
    Buffer_Unpin();
    g_state_user_error_count++;
    s_command_index = 0U;
    s_retry_at = HAL_GetTick() + MQTT_STARTUP_RETRY_MS;
    s_state = STARTUP;
    (void)LED_SetBlink(&Error_LED, MQTT_ERROR_BLINK_MS);
    LED_SetOff(&NET_State_LED);
}

void StateUser_Init(void)
{
    g_state_user_error_count = 0U;
    s_last_generation = 0U;
    s_retry_at = 0U;
    s_command_index = 0U;
    s_state = STARTUP;
    LED_SetOff(&Error_LED);
    LED_SetOff(&NET_State_LED);
    if ((SSID[0] == '\0') || (PASSWORD[0] == '\0') ||
        (HEAD_CAN_ID > 0x7FFU) || (TAIL_CAN_ID > 0x7FFU) ||
        (HEAD_CAN_ID == TAIL_CAN_ID) || (MQTT_BROKER_HOST[0] == '\0') ||
        (MQTT_TOPIC[0] == '\0') ||
        (ESP_AT_MQTT_BUFFER_SIZE_BYTE <
         (CAN_BLOCK_TX_SIZE + sizeof(MQTT_TOPIC) + 8U)))
    {
        s_state = CONFIG_REQUIRED;
        (void)LED_SetBlink(&Error_LED, MQTT_ERROR_BLINK_MS);
    }
}

void StateUser_Poll(void)
{
    MQTTAT_Result_t result = MQTTAT_GetResult();
    uint32_t generation;
    uint8_t index;
    LED_Update(&Error_LED);

    if ((s_state == SENDING) || (s_state == WAIT_RESULT))
    {
        if (MQTTAT_TakeTxDone() != 0U)
        {
            Buffer_Unpin();
            s_state = WAIT_RESULT;
        }
    }
    if (s_state == CONFIG_REQUIRED) return;
    if (s_state == STARTUP)
    {
        if ((int32_t)(HAL_GetTick() - s_retry_at) < 0) return;
        if (result == MQTTAT_RESULT_IDLE)
        {
            if (s_commands[s_command_index]() != HAL_OK) Restart();
            return;
        }
        if (result == MQTTAT_RESULT_PENDING) return;
        MQTTAT_ClearResult();
        if (result != MQTTAT_RESULT_OK) { Restart(); return; }
        s_command_index++;
        if (s_command_index == (sizeof(s_commands) / sizeof(s_commands[0])))
        {
            s_state = ONLINE;
            LED_SetOn(&NET_State_LED);
            LED_SetOff(&Error_LED);
        }
        return;
    }
    if (s_state == ONLINE)
    {
        if (MQTTAT_IsConnected() == 0U) { Restart(); return; }
        if (Buffer_HasNew(s_last_generation) != 0U)
        {
            if (MQTTAT_BeginPublish() == HAL_OK) s_state = WAIT_PROMPT;
            else Restart();
        }
        return;
    }
    if (s_state == WAIT_PROMPT)
    {
        if ((result == MQTTAT_RESULT_ERROR) || (result == MQTTAT_RESULT_TIMEOUT))
        { Restart(); return; }
        if (result != MQTTAT_RESULT_PROMPT) return;
        if (Buffer_PinLatest(s_last_generation, &index, &generation) == 0U) return;
        if (MQTTAT_SendPayload((uint8_t *)&can_block[index], CAN_BLOCK_TX_SIZE) != HAL_OK)
        { Restart(); return; }
        s_last_generation = generation;
        s_state = SENDING;
        return;
    }
    if ((s_state == SENDING) || (s_state == WAIT_RESULT))
    {
        if ((result == MQTTAT_RESULT_ERROR) || (result == MQTTAT_RESULT_TIMEOUT))
        { Restart(); return; }
        if ((result == MQTTAT_RESULT_OK) && (s_state == WAIT_RESULT))
        {
            MQTTAT_ClearResult();
            s_state = ONLINE;
        }
    }
}
