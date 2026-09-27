#ifndef LED_H
#define LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef enum
{
  LED_MODE_OFF = 0U,
  LED_MODE_ON,
  LED_MODE_BLINK
} LED_Mode_t;

typedef enum
{
  LED_OUTPUT_OFF = 0U,
  LED_OUTPUT_ON = 1U
} LED_OutputState_t;

typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
  GPIO_PinState active_level;
  LED_Mode_t mode;
  LED_OutputState_t output_state;
  uint32_t toggle_interval_ms;
  uint32_t last_toggle_ms;
} LED_Handle_t;

extern LED_Handle_t Error_LED;
extern LED_Handle_t NET_State_LED;
extern LED_Handle_t CAN_State_LED;
extern LED_Handle_t SYS_Tick_LED;
extern LED_Handle_t POWER_LED;

void LED_SetOn(LED_Handle_t *led);
void LED_SetOff(LED_Handle_t *led);
/* toggle_interval_ms is the time between output transitions. */
HAL_StatusTypeDef LED_SetBlink(LED_Handle_t *led, uint32_t toggle_interval_ms);
/* Call regularly from the main loop while the LED is in blink mode. */
void LED_Update(LED_Handle_t *led);
LED_OutputState_t LED_GetState(const LED_Handle_t *led);

#ifdef __cplusplus
}
#endif

#endif /* LED_H */
