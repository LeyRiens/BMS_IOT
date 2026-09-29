#include "state_user.h"
#include "buffer.h"
#include "config.h"
#include "led.h"
#include "mqtt_at.h"

typedef HAL_StatusTypeDef (*Command_t)(void);
typedef enum { STARTUP, ONLINE, WAIT_PROMPT, SENDING, WAIT_RESULT, CONFIG_REQUIRED, ESP_RESET_REQUIRED } State_t;
typedef enum { ERROR_LED_OFF, ERROR_LED_RETRY, ERROR_LED_CONFIG, ERROR_LED_RESET } ErrorLedCode_t;
typedef enum { ERROR_PHASE_ON, ERROR_PHASE_GAP, ERROR_PHASE_PAUSE } ErrorLedPhase_t;
static const Command_t s_commands[] = {
    MQTTAT_Test, MQTTAT_SetWifiModeStation, MQTTAT_JoinWifi,
    MQTTAT_SetMqttUserConfig, MQTTAT_SetMqttConnectionConfig, MQTTAT_ConnectMqtt
};
uint32_t g_state_user_error_count;
static State_t s_state;
static uint8_t s_command_index;
static uint32_t s_last_attempted_generation;
static uint32_t s_retry_at;
static uint8_t s_tx_snapshot[CAN_BLOCK_TX_SIZE];
static uint32_t s_snapshot_generation;
static uint32_t s_can_led_generation;
static uint32_t s_can_led_deadline;
static uint8_t s_can_led_active;
static ErrorLedCode_t s_error_led_code;
static ErrorLedPhase_t s_error_led_phase;
static uint8_t s_error_led_pulses_left;
static uint32_t s_error_led_deadline;

static uint8_t ErrorLedPulseCount(ErrorLedCode_t code)
{
    if (code == ERROR_LED_CONFIG) return ERROR_LED_CONFIG_PULSES;
    if (code == ERROR_LED_RESET) return ERROR_LED_RESET_PULSES;
    return ERROR_LED_RETRY_PULSES;
}

static uint32_t ErrorLedPause(ErrorLedCode_t code)
{
    return (code == ERROR_LED_RETRY) ? ERROR_LED_RETRY_PAUSE_MS : ERROR_LED_CODE_PAUSE_MS;
}

static void ErrorLedSet(ErrorLedCode_t code)
{
    if (s_error_led_code == code) return;
    s_error_led_code = code;
    s_error_led_pulses_left = 0U;
    if (code == ERROR_LED_OFF)
    {
        LED_SetOff(&Error_LED);
        return;
    }
    s_error_led_pulses_left = ErrorLedPulseCount(code);
    s_error_led_phase = ERROR_PHASE_ON;
    LED_SetOn(&Error_LED);
    s_error_led_deadline = HAL_GetTick() + ERROR_LED_PULSE_MS;
}

static void ErrorLedPoll(void)
{
    uint32_t now = HAL_GetTick();
    if ((s_error_led_code == ERROR_LED_OFF) ||
        ((int32_t)(now - s_error_led_deadline) < 0)) return;

    if (s_error_led_phase == ERROR_PHASE_ON)
    {
        LED_SetOff(&Error_LED);
        if (s_error_led_pulses_left > 1U)
        {
            s_error_led_pulses_left--;
            s_error_led_phase = ERROR_PHASE_GAP;
            s_error_led_deadline = now + ERROR_LED_GAP_MS;
        }
        else
        {
            s_error_led_pulses_left = 0U;
            s_error_led_phase = ERROR_PHASE_PAUSE;
            s_error_led_deadline = now + ErrorLedPause(s_error_led_code);
        }
    }
    else if (s_error_led_phase == ERROR_PHASE_GAP)
    {
        LED_SetOn(&Error_LED);
        s_error_led_phase = ERROR_PHASE_ON;
        s_error_led_deadline = now + ERROR_LED_PULSE_MS;
    }
    else
    {
        s_error_led_pulses_left = ErrorLedPulseCount(s_error_led_code);
        LED_SetOn(&Error_LED);
        s_error_led_phase = ERROR_PHASE_ON;
        s_error_led_deadline = now + ERROR_LED_PULSE_MS;
    }
}

static void CanLedPoll(void)
{
    uint32_t generation = Buffer_GetGeneration();
    uint32_t now = HAL_GetTick();
    if (generation != s_can_led_generation)
    {
        s_can_led_generation = generation;
        s_can_led_active = 1U;
        s_can_led_deadline = now + CAN_LED_PULSE_MS;
        LED_SetOn(&CAN_State_LED);
    }
    else if ((s_can_led_active != 0U) &&
             ((int32_t)(now - s_can_led_deadline) >= 0))
    {
        s_can_led_active = 0U;
        LED_SetOff(&CAN_State_LED);
    }
}

static void Restart(void)
{
    if (MQTTAT_Abort() != HAL_OK) Error_Handler();
    g_state_user_error_count++;
    s_command_index = 0U;
    s_retry_at = HAL_GetTick() + MQTT_STARTUP_RETRY_MS;
    s_state = (MQTTAT_NeedsEspReset() != 0U) ? ESP_RESET_REQUIRED : STARTUP;
    ErrorLedSet((s_state == ESP_RESET_REQUIRED) ? ERROR_LED_RESET : ERROR_LED_RETRY);
    LED_SetOff(&NET_State_LED);
}

void StateUser_Init(void)
{
    g_state_user_error_count = 0U;
    s_last_attempted_generation = 0U;
    s_retry_at = 0U;
    s_command_index = 0U;
    s_state = STARTUP;
    s_can_led_generation = Buffer_GetGeneration();
    s_can_led_active = 0U;
    s_error_led_code = ERROR_LED_OFF;
    LED_SetOff(&Error_LED);
    LED_SetOff(&NET_State_LED);
    LED_SetOff(&CAN_State_LED);
    if ((SSID[0] == '\0') || (PASSWORD[0] == '\0') ||
        (HEAD_CAN_ID > 0x7FFU) || (TAIL_CAN_ID > 0x7FFU) ||
        (HEAD_CAN_ID == TAIL_CAN_ID) || (MQTT_BROKER_HOST[0] == '\0') ||
        (MQTT_TOPIC[0] == '\0') ||
        (ESP_AT_MQTT_BUFFER_SIZE_BYTE <
         (CAN_BLOCK_TX_SIZE + sizeof(MQTT_TOPIC) + 8U)))
    {
        s_state = CONFIG_REQUIRED;
        ErrorLedSet(ERROR_LED_CONFIG);
    }
}

void StateUser_Poll(void)
{
    MQTTAT_Result_t result = MQTTAT_GetResult();
    CanLedPoll();
    ErrorLedPoll();

    if ((s_state == SENDING) || (s_state == WAIT_RESULT))
    {
        if (MQTTAT_TakeTxDone() != 0U)
        {
            s_state = WAIT_RESULT;
        }
    }
    if (s_state == CONFIG_REQUIRED) return;
    if (s_state == ESP_RESET_REQUIRED)
    {
        /* Only an observed ESP boot-ready response can release this lock. */
        if (MQTTAT_NeedsEspReset() == 0U)
        {
            s_retry_at = HAL_GetTick() + MQTT_STARTUP_RETRY_MS;
            s_state = STARTUP;
            ErrorLedSet(ERROR_LED_RETRY);
        }
        return;
    }
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
            ErrorLedSet(ERROR_LED_OFF);
        }
        return;
    }
    if (s_state == ONLINE)
    {
        if (MQTTAT_IsConnected() == 0U) { Restart(); return; }
        /* Latest-only: publish whenever idle, without a periodic send deadline. */
        if (Buffer_CopyLatest(s_last_attempted_generation, s_tx_snapshot,
                              &s_snapshot_generation) != 0U)
        {
            if (MQTTAT_BeginPublish(CAN_BLOCK_TX_SIZE) == HAL_OK) s_state = WAIT_PROMPT;
            else Restart();
        }
        return;
    }
    if (s_state == WAIT_PROMPT)
    {
        if ((result == MQTTAT_RESULT_ERROR) || (result == MQTTAT_RESULT_TIMEOUT))
        { Restart(); return; }
        if (result != MQTTAT_RESULT_PROMPT) return;
        /* Prefer a newer block at the prompt; retain the reserved snapshot if none exists. */
        (void)Buffer_CopyLatest(s_snapshot_generation, s_tx_snapshot, &s_snapshot_generation);
        if (MQTTAT_SendPayload(s_tx_snapshot, CAN_BLOCK_TX_SIZE) != HAL_OK)
        { Restart(); return; }
        s_last_attempted_generation = s_snapshot_generation;
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
