#ifndef MQTT_AT_H
#define MQTT_AT_H

#include "main.h"
#include "config.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    MQTTAT_RESULT_IDLE = 0U,
    MQTTAT_RESULT_PENDING,
    MQTTAT_RESULT_OK,
    MQTTAT_RESULT_ERROR,
    MQTTAT_RESULT_TIMEOUT
} MQTTAT_Result_t;

HAL_StatusTypeDef MQTTAT_Init(void);
void MQTTAT_Poll(void);
MQTTAT_Result_t MQTTAT_GetResult(void);
void MQTTAT_ClearResult(void);

HAL_StatusTypeDef MQTTAT_Test(void);
HAL_StatusTypeDef MQTTAT_SetWifiModeStation(void);
HAL_StatusTypeDef MQTTAT_JoinWifi(void);
HAL_StatusTypeDef MQTTAT_SetMqttUserConfig(void);
HAL_StatusTypeDef MQTTAT_SetMqttConnectionConfig(void);
HAL_StatusTypeDef MQTTAT_ConnectMqtt(void);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_AT_H */
