#ifndef MQTT_H
#define MQTT_H
#include "config.h"

/*========== 指令 ==========*/

#define AT_STRINGIFY_IMPL(x) #x
#define AT_STRINGIFY(x) AT_STRINGIFY_IMPL(x)

#define AT        "AT\r\n"

#define AT_WIFI_MODE    "AT+CWMODE=1\r\n"

#define AT_WIFI_CONNECT \
    "AT+CWJAP=\"" WIFI_SSID "\",\"" WIFI_PASSWORD "\"\r\n"

#define AT_MQTT_USERCFG \
    "AT+MQTTUSERCFG=0,1,\"" MQTT_CLIENT_ID "\",\"" \
    MQTT_USERNAME "\",\"" MQTT_PASSWORD "\",0,0,\"\"\r\n"

#define AT_MQTT_CONNCFG \
    "AT+MQTTCONNCFG=0,60,0,\"\",\"\",0,0\r\n"

#define AT_MQTT_CONNECT \
    "AT+MQTTCONN=0,\"" MQTT_HOST "\"," AT_STRINGIFY(MQTT_PORT) ",0\r\n"

#define AT_MQTT_VERIFY  "AT+MQTTCONN?\r\n"


/*========== 通用回复 ==========*/

#define READY           "ready"
#define OK              "OK"
#define ERROR           "ERROR"
#define BUSY            "busy p"
#define ERR_CODE        "ERR CODE:"

/*========== Wi-Fi 回复 ==========*/

#define WIFI_CONNECTED  "WIFI CONNECTED"
#define WIFI_GOT_IP     "WIFI GOT IP"
#define WIFI_DISCONNECT "WIFI DISCONNECT"
#define CWJAP_ERROR     "+CWJAP:"

/*========== MQTT 回复 ==========*/

#define MQTT_CONNECTED  "+MQTTCONNECTED:"
#define MQTT_DISCONNECT "+MQTTDISCONNECTED:"
#define MQTT_QUERY      "+MQTTCONN:"



#endif /* MQTT_H */