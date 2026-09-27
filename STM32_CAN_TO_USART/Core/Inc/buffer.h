#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>

#include "config.h"

#define DLC_8 8u


typedef enum {
    BUFFER_READY=0,
    BUFFER_FILLING,
    BUFFER_SENDING,
    BUFFER_EMPTY
} buffer_state_t;

/*
每个can帧放入1个frame_t结构体中。

每30个frame_t结构体放入block_t结构体中，
block_t是usart单次发送的数据，大小接近mqtt单次上传的上限

每4个block_t结构体放入buffer_t结构体中，
也就是BMS每0.1s的一次完整can发送的118个can帧将会放入同一个buffer里面
*/

typedef struct{
    uint32_t id;
    uint8_t data[FRAME_LENGTH];
}frame_t;

typedef struct{
    frame_t frame[FRAME_COUNT];
}block_t;

typedef struct buffer_t{
    block_t block[BLOCK_COUNT];
    buffer_state_t state;
    struct buffer_t *next_buffer;
}buffer_t;

typedef struct{
    buffer_t *current_buffer;
    uint8_t block_index;
    uint8_t frame_index;
}buffer_pointer_t;

extern buffer_pointer_t g_buffer_write;
extern buffer_pointer_t g_buffer_read;



void finalizeBuffer(buffer_t *buffer);

void bufferInit(void);

buffer_state_t getBufferState(buffer_t *buffer);

void updateFrame(buffer_pointer_t *pointer);


#endif //BUFFER_H