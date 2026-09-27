#include "led.h"

static void LED_Write(LED_Handle_t *led, LED_OutputState_t state)
{
  GPIO_PinState pin_level;

  if ((led == NULL) || (led->port == NULL))
  {
    return;
  }

  led->output_state = state;
  pin_level = (state == LED_OUTPUT_ON) ? led->active_level :
              ((led->active_level == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET);
  HAL_GPIO_WritePin(led->port, led->pin, pin_level);
}

LED_Handle_t Error_LED =
{
  .port = Error_GPIO_Port,
  .pin = Error_Pin,
  .active_level = GPIO_PIN_SET,
  .mode = LED_MODE_OFF,
  .output_state = LED_OUTPUT_OFF,
  .toggle_interval_ms = 0U,
  .last_toggle_ms = 0U
};

LED_Handle_t NET_State_LED =
{
  .port = NET_State_GPIO_Port,
  .pin = NET_State_Pin,
  .active_level = GPIO_PIN_SET,
  .mode = LED_MODE_OFF,
  .output_state = LED_OUTPUT_OFF,
  .toggle_interval_ms = 0U,
  .last_toggle_ms = 0U
};

LED_Handle_t CAN_State_LED =
{
  .port = CAN_State_GPIO_Port,
  .pin = CAN_State_Pin,
  .active_level = GPIO_PIN_SET,
  .mode = LED_MODE_OFF,
  .output_state = LED_OUTPUT_OFF,
  .toggle_interval_ms = 0U,
  .last_toggle_ms = 0U
};

LED_Handle_t SYS_Tick_LED =
{
  .port = SYS_Tick_GPIO_Port,
  .pin = SYS_Tick_Pin,
  .active_level = GPIO_PIN_SET,
  .mode = LED_MODE_OFF,
  .output_state = LED_OUTPUT_OFF,
  .toggle_interval_ms = 0U,
  .last_toggle_ms = 0U
};

LED_Handle_t POWER_LED =
{
  .port = POWER_GPIO_Port,
  .pin = POWER_Pin,
  .active_level = GPIO_PIN_SET,
  .mode = LED_MODE_OFF,
  .output_state = LED_OUTPUT_OFF,
  .toggle_interval_ms = 0U,
  .last_toggle_ms = 0U
};

void LED_SetOn(LED_Handle_t *led)
{
  if (led == NULL)
  {
    return;
  }

  led->mode = LED_MODE_ON;
  led->toggle_interval_ms = 0U;
  LED_Write(led, LED_OUTPUT_ON);
}

void LED_SetOff(LED_Handle_t *led)
{
  if (led == NULL)
  {
    return;
  }

  led->mode = LED_MODE_OFF;
  led->toggle_interval_ms = 0U;
  LED_Write(led, LED_OUTPUT_OFF);
}

HAL_StatusTypeDef LED_SetBlink(LED_Handle_t *led, uint32_t toggle_interval_ms)
{
  if ((led == NULL) || (led->port == NULL) || (toggle_interval_ms == 0U))
  {
    return HAL_ERROR;
  }

  led->mode = LED_MODE_BLINK;
  led->toggle_interval_ms = toggle_interval_ms;
  led->last_toggle_ms = HAL_GetTick();
  LED_Write(led, LED_OUTPUT_ON);
  return HAL_OK;
}

void LED_Update(LED_Handle_t *led)
{
  uint32_t elapsed_ms;
  uint32_t toggle_count;

  if ((led == NULL) || (led->mode != LED_MODE_BLINK) ||
      (led->toggle_interval_ms == 0U))
  {
    return;
  }

  elapsed_ms = HAL_GetTick() - led->last_toggle_ms;
  if (elapsed_ms < led->toggle_interval_ms)
  {
    return;
  }

  toggle_count = elapsed_ms / led->toggle_interval_ms;
  if ((toggle_count & 1U) != 0U)
  {
    LED_Write(led, (led->output_state == LED_OUTPUT_ON) ?
                    LED_OUTPUT_OFF : LED_OUTPUT_ON);
  }

  led->last_toggle_ms += toggle_count * led->toggle_interval_ms;
}

LED_OutputState_t LED_GetState(const LED_Handle_t *led)
{
  if (led == NULL)
  {
    return LED_OUTPUT_OFF;
  }

  return led->output_state;
}

