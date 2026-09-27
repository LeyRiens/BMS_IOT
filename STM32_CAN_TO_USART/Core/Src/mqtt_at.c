#include "mqtt_at.h"

#include "config.h"
#include "usart.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define MQTTAT_RX_SIZE             256U
#define MQTTAT_TX_SIZE             256U
#define MQTTAT_LINE_SIZE           256U
#define MQTTAT_COMMAND_TIMEOUT     3000U
#define MQTTAT_WIFI_TIMEOUT        30000U
#define MQTTAT_MQTT_TIMEOUT        60000U
#define MQTTAT_TX_TIMEOUT          1000U

typedef enum
{
    EXPECT_OK = 0U,
    EXPECT_WIFI,
    EXPECT_MQTT_CONNECTED
} ExpectedResponse_t;

static UART_HandleTypeDef *s_uart;
static uint8_t s_rx_buffer[MQTTAT_RX_SIZE];
static uint16_t s_rx_read;
static char s_line[MQTTAT_LINE_SIZE];
static uint16_t s_line_length;
static uint8_t s_line_overflow;
static char s_tx_buffer[MQTTAT_TX_SIZE];
static volatile MQTTAT_Result_t s_result;
static volatile uint8_t s_command_active;
static ExpectedResponse_t s_expected;
static uint8_t s_saw_ok;
static uint8_t s_saw_wifi_ip;
static uint32_t s_start_tick;
static uint32_t s_timeout;

static HAL_StatusTypeDef MQTTAT_StartCommand(
    ExpectedResponse_t expected,
    uint32_t timeout,
    const char *format,
    ...)
{
    va_list args;
    HAL_StatusTypeDef status;
    int length;

    if ((s_uart == NULL) || (s_command_active != 0U))
    {
        return HAL_BUSY;
    }

    va_start(args, format);
    length = vsnprintf(s_tx_buffer, sizeof(s_tx_buffer) - 2U, format, args);
    va_end(args);

    if ((length < 0) || (length >= (int)(sizeof(s_tx_buffer) - 2U)))
    {
        return HAL_ERROR;
    }

    s_tx_buffer[length++] = '\r';
    s_tx_buffer[length++] = '\n';

    s_expected = expected;
    s_saw_ok = 0U;
    s_saw_wifi_ip = 0U;
    s_line_length = 0U;
    s_line_overflow = 0U;
    s_timeout = timeout;
    s_result = MQTTAT_RESULT_PENDING;
    s_command_active = 1U;

    /* Polling TX avoids requiring a USART TC IRQ for HAL UART DMA completion. */
    status = HAL_UART_Transmit(
        s_uart,
        (uint8_t *)s_tx_buffer,
        (uint16_t)length,
        MQTTAT_TX_TIMEOUT);
    if (status != HAL_OK)
    {
        s_command_active = 0U;
        s_result = MQTTAT_RESULT_IDLE;
        return status;
    }

    s_start_tick = HAL_GetTick();
    return HAL_OK;
}

static uint8_t MQTTAT_HasPrefix(const char *text, const char *prefix)
{
    return (uint8_t)(strncmp(text, prefix, strlen(prefix)) == 0);
}

static void MQTTAT_ProcessLine(const char *line)
{
    if ((s_command_active == 0U) || (line[0] == '\0'))
    {
        return;
    }

    if ((strcmp(line, "ERROR") == 0) ||
        (strcmp(line, "FAIL") == 0) ||
        MQTTAT_HasPrefix(line, "ERROR:") ||
        MQTTAT_HasPrefix(line, "ERR CODE:") ||
        MQTTAT_HasPrefix(line, "busy "))
    {
        s_command_active = 0U;
        s_result = MQTTAT_RESULT_ERROR;
        return;
    }

    if (strcmp(line, "WIFI GOT IP") == 0)
    {
        s_saw_wifi_ip = 1U;
    }
    if (strcmp(line, "OK") == 0)
    {
        s_saw_ok = 1U;
    }

    if (((s_expected == EXPECT_OK) && (s_saw_ok != 0U)) ||
        ((s_expected == EXPECT_WIFI) &&
         (s_saw_ok != 0U) && (s_saw_wifi_ip != 0U)) ||
        ((s_expected == EXPECT_MQTT_CONNECTED) &&
         MQTTAT_HasPrefix(line, "+MQTTCONNECTED:0")))
    {
        s_command_active = 0U;
        s_result = MQTTAT_RESULT_OK;
    }
}

static void MQTTAT_ProcessByte(uint8_t byte)
{
    if ((byte == '\r') || (byte == '\n'))
    {
        if ((s_line_overflow == 0U) && (s_line_length != 0U))
        {
            s_line[s_line_length] = '\0';
            MQTTAT_ProcessLine(s_line);
        }
        s_line_length = 0U;
        s_line_overflow = 0U;
        return;
    }

    if ((byte == '\0') || (s_line_overflow != 0U))
    {
        return;
    }

    if (s_line_length >= (uint16_t)(sizeof(s_line) - 1U))
    {
        s_line_overflow = 1U;
        return;
    }

    s_line[s_line_length++] = (char)byte;
}

HAL_StatusTypeDef MQTTAT_Init(void)
{
    s_uart = &huart1;
    s_rx_read = 0U;
    s_line_length = 0U;
    s_line_overflow = 0U;
    s_result = MQTTAT_RESULT_IDLE;
    s_command_active = 0U;

    if (s_uart->hdmarx == NULL)
    {
        return HAL_ERROR;
    }

    return HAL_UART_Receive_DMA(s_uart, s_rx_buffer, (uint16_t)sizeof(s_rx_buffer));
}

void MQTTAT_Poll(void)
{
    uint16_t write_index;
    uint16_t remaining;

    if ((s_uart == NULL) || (s_uart->hdmarx == NULL))
    {
        return;
    }

    remaining = (uint16_t)__HAL_DMA_GET_COUNTER(s_uart->hdmarx);
    write_index = (uint16_t)(sizeof(s_rx_buffer) - remaining);
    if (write_index >= (uint16_t)sizeof(s_rx_buffer))
    {
        write_index = 0U;
    }

    while (s_rx_read != write_index)
    {
        MQTTAT_ProcessByte(s_rx_buffer[s_rx_read]);
        s_rx_read++;
        if (s_rx_read >= (uint16_t)sizeof(s_rx_buffer))
        {
            s_rx_read = 0U;
        }
    }

    if ((s_command_active != 0U) &&
        ((HAL_GetTick() - s_start_tick) >= s_timeout))
    {
        s_command_active = 0U;
        s_result = MQTTAT_RESULT_TIMEOUT;
    }
}

MQTTAT_Result_t MQTTAT_GetResult(void)
{
    return s_result;
}

void MQTTAT_ClearResult(void)
{
    if (s_command_active == 0U)
    {
        s_result = MQTTAT_RESULT_IDLE;
    }
}

HAL_StatusTypeDef MQTTAT_Test(void)
{
    return MQTTAT_StartCommand(EXPECT_OK, MQTTAT_COMMAND_TIMEOUT, "AT");
}

HAL_StatusTypeDef MQTTAT_SetWifiModeStation(void)
{
    return MQTTAT_StartCommand(EXPECT_OK, MQTTAT_COMMAND_TIMEOUT, "AT+CWMODE=1");
}

HAL_StatusTypeDef MQTTAT_JoinWifi(void)
{
    return MQTTAT_StartCommand(
        EXPECT_WIFI,
        MQTTAT_WIFI_TIMEOUT,
        "AT+CWJAP=\"%s\",\"%s\"",
        SSID,
        PASSWORD);
}

HAL_StatusTypeDef MQTTAT_SetMqttUserConfig(void)
{
    return MQTTAT_StartCommand(
        EXPECT_OK,
        MQTTAT_COMMAND_TIMEOUT,
        "AT+MQTTUSERCFG=0,%u,\"%s\",\"%s\",\"%s\",0,0,\"\"",
        MQTT_SCHEME,
        MQTT_CLIENT_ID,
        MQTT_USERNAME,
        MQTT_PASSWORD);
}

HAL_StatusTypeDef MQTTAT_SetMqttConnectionConfig(void)
{
    return MQTTAT_StartCommand(
        EXPECT_OK,
        MQTTAT_COMMAND_TIMEOUT,
        "AT+MQTTCONNCFG=0,%u,0,\"\",\"\",0,0",
        MQTT_KEEPALIVE_SECONDS);
}

HAL_StatusTypeDef MQTTAT_ConnectMqtt(void)
{
    return MQTTAT_StartCommand(
        EXPECT_MQTT_CONNECTED,
        MQTTAT_MQTT_TIMEOUT,
        "AT+MQTTCONN=0,\"%s\",%u,0",
        MQTT_BROKER_HOST,
        MQTT_BROKER_PORT);
}