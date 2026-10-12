#include "buffer.h"



static buffer_t g_buffers[BUFFER_COUNT];

static uint16_t g_batch_sequence;



buffer_pointer_t g_buffer_write=
{
    .current_buffer=&g_buffers[0],
    .block_index=0u,
    .frame_index=0u
};

buffer_pointer_t g_buffer_read=
{
    .current_buffer=&g_buffers[0],
    .block_index=0u,
    .frame_index=0u
};


/*
每一帧写入或读取后调用函数updateFrame更新下一块要写入的frame
*/

void updateBuffer(buffer_pointer_t *pointer)
{
    pointer->current_buffer = pointer->current_buffer->next_buffer;
    g_buffer_write.block_index = 0u;
    g_buffer_write.frame_index = 0u;
}

void updateBlock(buffer_pointer_t *pointer)
{
    pointer->block_index++;
   
    if(pointer->block_index >= BLOCK_COUNT)
    {
        pointer->block_index = 0u;
    }
}

void updateFrame(buffer_pointer_t *pointer)
{
    pointer->frame_index++;
    
    if(pointer->frame_index >= FRAME_COUNT)
    {
        pointer->frame_index = 0u;
       
        updateBlock(pointer);
    }
}





void finalizeBuffer(buffer_t *buffer)
{
    block_t *last_block =
        &buffer->block[BLOCK_COUNT - 1u];

    last_block->frame[FRAME_COUNT - 2u].data[0] =
        (uint8_t)((g_batch_sequence >> 8) & 0xFFu);

    last_block->frame[FRAME_COUNT - 1u].data[0] =
        (uint8_t)(g_batch_sequence & 0xFFu);

    g_batch_sequence++;

   updateBuffer(&g_buffer_write);
}

void bufferInit(void)
{
    uint8_t i;
    uint8_t next_index;

    for (i = 0u; i < BUFFER_COUNT; i++)
    {
        next_index = i + 1u;

        if (next_index == BUFFER_COUNT)
        {
            next_index = 0u;
        }

        g_buffers[i].state =
            BUFFER_EMPTY;

        g_buffers[i].next_buffer =
            &g_buffers[next_index];
    }
}

buffer_state_t getBufferState(buffer_t *buffer)
{
    return buffer->state;
}

