#ifndef STATE_USER_H
#define STATE_USER_H

#include <stdint.h>
#include "led.h"
#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t g_state_user_error_count;

void StateUser_Init(void);
void StateUser_Poll(void);
void mqttatconnecterror(void);

#ifdef __cplusplus
}
#endif

#endif /* STATE_USER_H */
