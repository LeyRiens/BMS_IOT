#include "state_user.h"

#include "led.h"
#include "mqtt_at.h"

#define STATE_ERROR_LIMIT       5U
#define ERROR_LED_BLINK_MS      500U

typedef HAL_StatusTypeDef (*StateCommand_t)(void);

typedef enum
{
    STATE_RUNNING = 0U,
    STATE_CONNECTED,
    STATE_CONNECT_ERROR
} State_t;

static const StateCommand_t s_commands[] =
{
    MQTTAT_Test,
    MQTTAT_SetWifiModeStation,
    MQTTAT_JoinWifi,
    MQTTAT_SetMqttUserConfig,
    MQTTAT_SetMqttConnectionConfig,
    MQTTAT_ConnectMqtt
};

uint32_t g_state_user_error_count = 0U;

static State_t s_state;
static uint8_t s_command_index;

void mqttatconnecterror(void)
{
    if (Error_LED.mode != LED_MODE_BLINK)
    {
        (void)LED_SetBlink(&Error_LED, ERROR_LED_BLINK_MS);
    }
}

static void State_RecordError(void)
{
    if (g_state_user_error_count < 0xFFFFFFFFUL)
    {
        g_state_user_error_count++;
    }

    if (g_state_user_error_count > STATE_ERROR_LIMIT)
    {
        s_state = STATE_CONNECT_ERROR;
        mqttatconnecterror();
    }
    else
    {
        s_command_index = 0U;
    }
}

void StateUser_Init(void)
{
    g_state_user_error_count = 0U;
    s_command_index = 0U;
    s_state = STATE_RUNNING;
}

void StateUser_Poll(void)
{
    MQTTAT_Result_t result;

    LED_Update(&Error_LED);
    if (s_state != STATE_RUNNING)
    {
        return;
    }

    result = MQTTAT_GetResult();
    if (result == MQTTAT_RESULT_IDLE)
    {
        if (s_commands[s_command_index]() != HAL_OK)
        {
            State_RecordError();
        }
        return;
    }

    if (result == MQTTAT_RESULT_PENDING)
    {
        return;
    }

    MQTTAT_ClearResult();
    if (result != MQTTAT_RESULT_OK)
    {
        State_RecordError();
        return;
    }

    s_command_index++;
    if (s_command_index >= (uint8_t)(sizeof(s_commands) / sizeof(s_commands[0])))
    {
        s_state = STATE_CONNECTED;
    }
}