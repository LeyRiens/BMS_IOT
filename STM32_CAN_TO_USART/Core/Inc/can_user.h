#ifndef CAN_USER_H
#define CAN_USER_H
#include "buffer.h"
#include "can.h"
#include <stm32f1xx_hal_can.h>




extern buffer_pointer_t g_buffer_write;
extern buffer_pointer_t g_buffer_read;



HAL_StatusTypeDef CAN_UserInit(void);







#endif //CAN_USER_H
