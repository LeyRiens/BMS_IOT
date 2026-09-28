#ifndef STATE_USER_H
#define STATE_USER_H
#include <stdint.h>
extern uint32_t g_state_user_error_count;
void StateUser_Init(void);
void StateUser_Poll(void);
#endif
