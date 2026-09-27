#ifndef CONFIG_H
#define CONFIG_H

/* Wi-Fi */
#define SSID                    "LYY"
#define PASSWORD                "1234567890"

/* MQTT connection: fill in the broker host and optional credentials. */
#define MQTT_BROKER_HOST        ""
#define MQTT_BROKER_PORT        1883U
#define MQTT_CLIENT_ID          "STM32_CAN_TO_MQTT"
#define MQTT_USERNAME           ""
#define MQTT_PASSWORD           ""
#define MQTT_SCHEME             1U
#define MQTT_KEEPALIVE_SECONDS  60U

/* ESP-AT UART */
#define MQTT_AT_UART_BAUDRATE   460800U

/* CAN buffering */
#define FRAME_LENGTH 8
#define FRAME_COUNT 32
#define BLOCK_COUNT 4
#define BUFFER_COUNT 4

#endif /* CONFIG_H */
