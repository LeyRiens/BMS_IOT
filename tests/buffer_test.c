#include "buffer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void push_block(uint8_t marker)
{
    uint8_t data[8] = {0};
    unsigned i;
    data[0] = marker;
    Buffer_ReceiveFrame(0x120U, data);
    for (i = 1U; i < 125U; i++)
        Buffer_ReceiveFrame(0x121U + i, data);
    Buffer_ReceiveFrame(0x1A0U, data);
}

int main(void)
{
    uint8_t index;
    uint32_t generation;
    uint8_t data[8] = {0};
    unsigned i;
    Buffer_Init();
    Buffer_ReceiveFrame(0x1A0U, data);
    assert(latest_ready_idx == CAN_NO_BLOCK);
    push_block(7U);
    assert(write_idx == 1U && latest_ready_idx == 0U);
    assert(global_block_seq == 1U && can_block[0].block_seq == 0U);
    assert(can_block[0].frame[0].id == 0x120U);
    assert(can_block[0].frame[125].id == 0x1A0U);
    assert(can_block[0].frame[62].data[0] == 7U);
    assert(Buffer_PinLatest(0U, &index, &generation) == 1U);
    assert(index == 0U && generation == 1U && tx_block_idx == 0);

    for (i = 0U; i < 3U; i++) push_block((uint8_t)(8U + i));
    assert(write_idx == 0U && latest_ready_idx == 3U);
    push_block(11U); /* DMA still reads B0: discard this cycle. */
    assert(write_idx == 1U && global_block_seq == 4U);
    assert(can_block[0].frame[62].data[0] == 7U);
    assert(can_tx_collision_count == 1U);
    Buffer_Unpin();
    assert(Buffer_PinLatest(generation, &index, &generation) == 1U);
    assert(index == 3U && generation == 4U);
    Buffer_Unpin();

    Buffer_Init();
    Buffer_ReceiveFrame(0x120U, data);
    Buffer_ReceiveFrame(0x1A0U, data);
    assert(can_invalid_block_count == 1U);
    assert(global_block_seq == 0U);
    push_block(12U);
    assert(global_block_seq == 1U);
    Buffer_Init();
    global_block_seq = 65535U;
    push_block(13U);
    assert(can_block[0].block_seq == 65535U);
    push_block(14U);
    assert(can_block[1].block_seq == 0U && global_block_seq == 1U);

    Buffer_Init();
    push_block(15U);
    for (i = 0U; i < 3U; i++)
    {
        Buffer_ReceiveFrame(0x120U, data);
        Buffer_ReceiveFrame(0x1A0U, data);
    }
    assert(write_idx == 0U && latest_ready_idx == 0U);
    Buffer_ReceiveFrame(0x120U, data);
    assert(Buffer_HasNew(0U) == 0U); /* B0 is no longer a complete snapshot. */
    puts("buffer_test: OK");
    return 0;
}
