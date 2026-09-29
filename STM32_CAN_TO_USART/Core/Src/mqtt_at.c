#include "mqtt_at.h"
#include "config.h"
#include "usart.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef enum { KIND_OK, KIND_CONNECT, KIND_PUBLISH } Kind_t;
typedef enum { PHASE_IDLE, PHASE_COMMAND, PHASE_PROMPT, PHASE_DMA, PHASE_RESULT } Phase_t;
static uint8_t s_rx[MQTT_AT_RX_SIZE];
static uint16_t s_read;
static char s_line[MQTT_AT_LINE_SIZE];
static uint16_t s_line_len;
static uint8_t s_overflow;
static char s_command[MQTT_AT_COMMAND_SIZE];
static volatile uint8_t s_tx_done;
static volatile uint8_t s_uart_error;
/* Only Poll consumes the IRQ flag; the application consumes this separate event. */
static uint8_t s_tx_complete;
/* A published RAW command may leave the ESP waiting for payload bytes. */
static uint8_t s_raw_pending;
static uint16_t s_payload_length;
static volatile uint8_t s_dma_active;
static volatile uint8_t s_connected;
static MQTTAT_Result_t s_result;
static Kind_t s_kind;
static Phase_t s_phase;
static uint8_t s_pub_ok;
static uint32_t s_started;
static uint32_t s_timeout;

static void Finish(MQTTAT_Result_t result)
{
    s_phase = PHASE_IDLE;
    s_result = result;
}

static uint8_t Prefix(const char *line, const char *prefix)
{
    return (uint8_t)(strncmp(line, prefix, strlen(prefix)) == 0);
}

static void ProcessLine(const char *line)
{
    if (Prefix(line, "+MQTTDISCONNECTED:0") ||
        (strcmp(line, "WIFI DISCONNECT") == 0) || (strcmp(line, "ready") == 0))
    {
        s_connected = 0U;
        if (strcmp(line, "ready") == 0) s_raw_pending = 0U;
        if (s_kind == KIND_PUBLISH && s_phase != PHASE_IDLE)
            Finish(MQTTAT_RESULT_ERROR);
    }
    if (Prefix(line, "+MQTTCONNECTED:0")) s_connected = 1U;
    if ((s_phase == PHASE_IDLE) || (line[0] == '\0')) return;
    if ((strcmp(line, "ERROR") == 0) || (strcmp(line, "FAIL") == 0) ||
        Prefix(line, "ERROR:") || Prefix(line, "ERR CODE:") ||
        Prefix(line, "busy ") || Prefix(line, "+MQTTPUB:FAIL"))
    {
        /* An explicit rejection before the prompt never entered RAW data mode. */
        if ((s_phase == PHASE_PROMPT) || Prefix(line, "+MQTTPUB:FAIL"))
            s_raw_pending = 0U;
        Finish(MQTTAT_RESULT_ERROR);
        return;
    }
    if (s_kind == KIND_PUBLISH)
    {
        if (Prefix(line, "+MQTTPUB:OK") &&
            ((s_phase == PHASE_DMA) || (s_phase == PHASE_RESULT)))
        {
            s_raw_pending = 0U;
            s_pub_ok = 1U;
            if (s_phase == PHASE_RESULT) Finish(MQTTAT_RESULT_OK);
        }
    }
    else if ((s_kind == KIND_OK) && (strcmp(line, "OK") == 0))
        Finish(MQTTAT_RESULT_OK);
    else if ((s_kind == KIND_CONNECT) && Prefix(line, "+MQTTCONNECTED:0"))
        Finish(MQTTAT_RESULT_OK);
}

static void ProcessByte(uint8_t byte)
{
    if ((s_phase == PHASE_PROMPT) && (byte == '>') &&
        (s_line_len == 0U) && (s_overflow == 0U))
    {
        s_phase = PHASE_COMMAND;
        s_result = MQTTAT_RESULT_PROMPT;
        s_line_len = 0U;
        return;
    }
    if ((byte == '\r') || (byte == '\n'))
    {
        if ((s_overflow == 0U) && (s_line_len != 0U))
        {
            s_line[s_line_len] = '\0';
            ProcessLine(s_line);
        }
        s_line_len = 0U;
        s_overflow = 0U;
        return;
    }
    if ((byte == 0U) || (s_overflow != 0U)) return;
    if (s_line_len >= MQTT_AT_LINE_SIZE - 1U) { s_overflow = 1U; return; }
    s_line[s_line_len++] = (char)byte;
}

static HAL_StatusTypeDef Start(Kind_t kind, uint32_t timeout, const char *format, ...)
{
    va_list args;
    int length;
    HAL_StatusTypeDef status;
    if ((s_result != MQTTAT_RESULT_IDLE) || (s_raw_pending != 0U) ||
        (s_uart_error != 0U)) return HAL_BUSY;
    va_start(args, format);
    length = vsnprintf(s_command, MQTT_AT_COMMAND_SIZE - 2U, format, args);
    va_end(args);
    if ((length < 0) || (length >= (int)(MQTT_AT_COMMAND_SIZE - 2U))) return HAL_ERROR;
    s_command[length++] = '\r';
    s_command[length++] = '\n';
    s_kind = kind;
    s_pub_ok = 0U;
    s_line_len = 0U;
    s_overflow = 0U;
    s_timeout = timeout;
    s_started = HAL_GetTick();
    s_phase = (kind == KIND_PUBLISH) ? PHASE_PROMPT : PHASE_COMMAND;
    s_result = MQTTAT_RESULT_PENDING;
    if (kind == KIND_PUBLISH) s_raw_pending = 1U;
    status = HAL_UART_Transmit(&huart1, (uint8_t *)s_command, (uint16_t)length,
                               MQTT_AT_COMMAND_TX_TIMEOUT_MS);
    if (status != HAL_OK) { s_phase = PHASE_IDLE; s_result = MQTTAT_RESULT_IDLE; }
    return status;
}

static HAL_StatusTypeDef StartReceive(void)
{
    s_read = 0U;
    s_line_len = 0U;
    s_overflow = 0U;
    return HAL_UART_Receive_DMA(&huart1, s_rx, MQTT_AT_RX_SIZE);
}

HAL_StatusTypeDef MQTTAT_Init(void)
{
    s_read = 0U;
    s_line_len = 0U;
    s_overflow = 0U;
    s_tx_done = 0U;
    s_uart_error = 0U;
    s_tx_complete = 0U;
    s_raw_pending = 0U;
    s_payload_length = 0U;
    s_dma_active = 0U;
    s_connected = 0U;
    s_phase = PHASE_IDLE;
    s_result = MQTTAT_RESULT_IDLE;
    if ((huart1.hdmarx == NULL) ||
        (huart1.hdmarx->Init.Mode != DMA_CIRCULAR)) return HAL_ERROR;
    /* HAL UART normal-mode DMA TX finishes on USART TC, after DMA IRQ. */
    HAL_NVIC_SetPriority(USART1_IRQn, MQTT_AT_USART_IRQ_PRIORITY, 0U);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    return StartReceive();
}

void MQTTAT_Poll(void)
{
    uint16_t end;
    if (huart1.hdmarx == NULL) return;
    if (s_uart_error != 0U)
    {
        /* HAL stops RX DMA on receive errors. Recover independently of TX phase. */
        s_uart_error = 0U;
        s_connected = 0U;
        if ((HAL_UART_AbortReceive(&huart1) != HAL_OK) || (StartReceive() != HAL_OK))
            s_uart_error = 1U; /* Retry from the main loop; never start another command. */
        Finish(MQTTAT_RESULT_ERROR);
        return;
    }
    if ((s_dma_active != 0U) && (s_tx_done != 0U))
    {
        s_tx_done = 0U;
        s_dma_active = 0U;
        s_tx_complete = 1U;
        if (s_phase == PHASE_DMA)
        {
            s_phase = PHASE_RESULT;
            if (s_pub_ok != 0U) Finish(MQTTAT_RESULT_OK);
        }
    }
    end = (uint16_t)(MQTT_AT_RX_SIZE - __HAL_DMA_GET_COUNTER(huart1.hdmarx));
    if (end == MQTT_AT_RX_SIZE) end = 0U;
    while (s_read != end)
    {
        ProcessByte(s_rx[s_read]);
        s_read++;
        if (s_read == MQTT_AT_RX_SIZE) s_read = 0U;
    }
    if ((s_phase != PHASE_IDLE) && ((HAL_GetTick() - s_started) >= s_timeout))
        Finish(MQTTAT_RESULT_TIMEOUT);
}

MQTTAT_Result_t MQTTAT_GetResult(void) { return s_result; }
void MQTTAT_ClearResult(void)
{
    if (s_phase == PHASE_IDLE) s_result = MQTTAT_RESULT_IDLE;
}
uint8_t MQTTAT_IsConnected(void) { return s_connected; }
uint8_t MQTTAT_NeedsEspReset(void) { return s_raw_pending; }
uint8_t MQTTAT_TakeTxDone(void)
{
    uint8_t done = s_tx_complete;
    s_tx_complete = 0U;
    return done;
}
HAL_StatusTypeDef MQTTAT_Abort(void)
{
    if (s_dma_active != 0U)
    {
        if (HAL_UART_AbortTransmit(&huart1) != HAL_OK) return HAL_ERROR;
        s_dma_active = 0U;
    }
    s_phase = PHASE_IDLE;
    s_result = MQTTAT_RESULT_IDLE;
    s_tx_done = 0U;
    s_tx_complete = 0U;
    /* Preserve RX errors and RAW uncertainty across a local transaction abort. */
    return HAL_OK;
}

HAL_StatusTypeDef MQTTAT_Test(void)
{ return Start(KIND_OK, MQTT_AT_COMMAND_TIMEOUT_MS, "AT"); }
HAL_StatusTypeDef MQTTAT_SetWifiModeStation(void)
{ return Start(KIND_OK, MQTT_AT_COMMAND_TIMEOUT_MS, "AT+CWMODE=1"); }
HAL_StatusTypeDef MQTTAT_JoinWifi(void)
{ return Start(KIND_OK, MQTT_AT_WIFI_TIMEOUT_MS, "AT+CWJAP=\"%s\",\"%s\"", SSID, PASSWORD); }
HAL_StatusTypeDef MQTTAT_SetMqttUserConfig(void)
{ return Start(KIND_OK, MQTT_AT_COMMAND_TIMEOUT_MS, "AT+MQTTUSERCFG=0,%u,\"%s\",\"%s\",\"%s\",0,0,\"\"",
               MQTT_SCHEME, MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD); }
HAL_StatusTypeDef MQTTAT_SetMqttConnectionConfig(void)
{ return Start(KIND_OK, MQTT_AT_COMMAND_TIMEOUT_MS, "AT+MQTTCONNCFG=0,%u,0,\"\",\"\",0,0",
               MQTT_KEEPALIVE_SECONDS); }
HAL_StatusTypeDef MQTTAT_ConnectMqtt(void)
{ return Start(KIND_CONNECT, MQTT_AT_CONNECT_TIMEOUT_MS, "AT+MQTTCONN=0,\"%s\",%u,0",
               MQTT_BROKER_HOST, MQTT_BROKER_PORT); }
HAL_StatusTypeDef MQTTAT_BeginPublish(uint16_t length)
{
    if ((length == 0U) ||
        ((uint32_t)length + sizeof(MQTT_TOPIC) + 8U > ESP_AT_MQTT_BUFFER_SIZE_BYTE))
        return HAL_ERROR;
    if (s_result != MQTTAT_RESULT_IDLE) return HAL_BUSY;
    s_payload_length = length;
    return Start(KIND_PUBLISH, MQTT_AT_PUBLISH_TIMEOUT_MS,
                 "AT+MQTTPUBRAW=0,\"%s\",%u,%u,%u",
                 MQTT_TOPIC, (unsigned)length, MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN);
}
HAL_StatusTypeDef MQTTAT_SendPayload(uint8_t *data, uint16_t length)
{
    HAL_StatusTypeDef status;
    if ((s_result != MQTTAT_RESULT_PROMPT) || (data == NULL) ||
        (length != s_payload_length) || (s_uart_error != 0U))
        return HAL_ERROR;
    s_tx_done = 0U;
    s_tx_complete = 0U;
    s_dma_active = 1U;
    s_phase = PHASE_DMA;
    s_result = MQTTAT_RESULT_PENDING;
    s_started = HAL_GetTick();
    status = HAL_UART_Transmit_DMA(&huart1, data, length);
    if (status != HAL_OK) { s_dma_active = 0U; Finish(MQTTAT_RESULT_ERROR); }
    return status;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{ if (uart == &huart1) s_tx_done = 1U; }
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{ if (uart == &huart1) s_uart_error = 1U; }
