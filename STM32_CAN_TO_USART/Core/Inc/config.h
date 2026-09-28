#ifndef CONFIG_H
#define CONFIG_H

/* Wi-Fi */
#define SSID                    ""
#define PASSWORD                ""

/* MQTT connection and publish settings. */
#define MQTT_BROKER_HOST        ""
#define MQTT_BROKER_PORT        1883U
#define MQTT_CLIENT_ID          "STM32_CAN_TO_MQTT"
#define MQTT_USERNAME           ""
#define MQTT_PASSWORD           ""
#define MQTT_SCHEME             1U
#define MQTT_KEEPALIVE_SECONDS  60U
#define MQTT_TOPIC              ""
#define MQTT_PUBLISH_QOS        0U
#define MQTT_PUBLISH_RETAIN     0U

/* Match the value in the flashed ESP8266 ESP-AT firmware, including MQTT header. */
#define ESP_AT_MQTT_BUFFER_SIZE_BYTE 512U

/* ESP-AT UART, parser and transaction timing. */
#define MQTT_AT_UART_BAUDRATE   460800U
#define MQTT_AT_USART_IRQ_PRIORITY 1U
#define MQTT_AT_RX_SIZE         256U
#define MQTT_AT_LINE_SIZE       128U
#define MQTT_AT_COMMAND_SIZE    256U
#define MQTT_AT_COMMAND_TIMEOUT_MS 3000U
#define MQTT_AT_WIFI_TIMEOUT_MS 30000U
#define MQTT_AT_CONNECT_TIMEOUT_MS 60000U
#define MQTT_AT_PUBLISH_TIMEOUT_MS 15000U
#define MQTT_AT_COMMAND_TX_TIMEOUT_MS 1000U
#define MQTT_STARTUP_RETRY_MS   1000U
#define MQTT_ERROR_BLINK_MS     500U

/* CAN wire contract. Keep the sizes in sync with the 1514-byte block layout. */
/* Values above 0x7FF disable capture until IDs are supplied. */
#define HEAD_CAN_ID             0xFFFFU
#define TAIL_CAN_ID             0xFFFFU
#define CAN_FRAME_DATA_SIZE     8U
#define CAN_FRAME_COUNT         126U
#define CAN_BUFFER_COUNT        4U
#define CAN_BLOCK_TX_SIZE       1514U

#endif /* CONFIG_H */
