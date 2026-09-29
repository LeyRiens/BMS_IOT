#include "buffer.h"
#include "config.h"
#include "stm32f1xx_hal.h"
#include <stddef.h>
#include <string.h>

typedef char FrameLayoutCheck[(sizeof(CAN_Frame_t) == 12U) ? 1 : -1];
typedef char BufferCountCheck[(CAN_BUFFER_COUNT == 4U) ? 1 : -1];
typedef char BlockLayoutCheck[(offsetof(CAN_Block_t, block_seq) == 1512U) ? 1 : -1];
typedef char TxLengthCheck[(CAN_BLOCK_TX_SIZE == 1514U) ? 1 : -1];

CAN_Block_t can_block[CAN_BUFFER_COUNT];
volatile uint8_t write_idx;
volatile uint8_t latest_ready_idx;
volatile uint16_t global_block_seq;
volatile int8_t tx_block_idx;
volatile uint32_t can_invalid_block_count;
volatile uint32_t can_tx_collision_count;
static volatile uint8_t s_receiving;
static volatile uint8_t s_discard;
static volatile uint8_t s_count;
static volatile uint8_t s_latest_valid;
static volatile uint32_t s_generation;

void Buffer_Init(void)
{
    write_idx = 0U;
    latest_ready_idx = CAN_NO_BLOCK;
    global_block_seq = 0U;
    tx_block_idx = -1;
    can_invalid_block_count = 0U;
    can_tx_collision_count = 0U;
    s_receiving = 0U;
    s_discard = 0U;
    s_count = 0U;
    s_latest_valid = 0U;
    s_generation = 0U;
}

static void Buffer_Advance(void)
{
    write_idx = (uint8_t)((write_idx + 1U) & 3U);
    s_receiving = 0U;
    s_discard = 0U;
    s_count = 0U;
}

/* Called from CAN FIFO0 IRQ. Only an exact 126-frame HEAD..TAIL block is published. */
void Buffer_ReceiveFrame(uint32_t id, const uint8_t data[8])
{
    CAN_Frame_t *frame;
    if ((HEAD_CAN_ID > 0x7FFU) || (TAIL_CAN_ID > 0x7FFU) ||
        (HEAD_CAN_ID == TAIL_CAN_ID)) return;

    if (id == HEAD_CAN_ID)
    {
        if (s_receiving != 0U) can_invalid_block_count++;
        s_receiving = 1U;
        s_count = 0U;
        s_discard = (tx_block_idx == (int8_t)write_idx) ? 1U : 0U;
        if (s_discard != 0U) can_tx_collision_count++;
        if ((s_latest_valid != 0U) && (latest_ready_idx == write_idx))
            s_latest_valid = 0U;
    }
    if (s_receiving == 0U) return;
    if ((s_discard == 0U) && (s_count < CAN_FRAME_COUNT))
    {
        frame = &can_block[write_idx].frame[s_count];
        frame->id = id;
        memcpy(frame->data, data, CAN_FRAME_DATA_SIZE);
    }
    if (s_count < CAN_FRAME_COUNT) s_count++;
    if (id == TAIL_CAN_ID)
    {
        if ((s_discard == 0U) && (s_count == CAN_FRAME_COUNT))
        {
            can_block[write_idx].block_seq = global_block_seq++;
            latest_ready_idx = write_idx;
            s_generation++;
            s_latest_valid = 1U;
        }
        else can_invalid_block_count++;
        Buffer_Advance();
    }
    else if (s_count >= CAN_FRAME_COUNT) s_discard = 1U;
}

uint8_t Buffer_HasNew(uint32_t after_generation)
{
    uint8_t result;
    uint32_t irq = __get_PRIMASK();
    __disable_irq();
    result = (s_latest_valid != 0U) && (s_generation != after_generation);
    __set_PRIMASK(irq);
    return result;
}

uint8_t Buffer_PinLatest(uint32_t after_generation, uint8_t *index, uint32_t *generation)
{
    uint8_t result = 0U;
    uint32_t irq = __get_PRIMASK();
    __disable_irq();
    if ((index != NULL) && (generation != NULL) && (tx_block_idx < 0) &&
        (s_latest_valid != 0U) && (s_generation != after_generation))
    {
        *index = latest_ready_idx;
        *generation = s_generation;
        tx_block_idx = (int8_t)latest_ready_idx;
        result = 1U;
    }
    __set_PRIMASK(irq);
    return result;
}

void Buffer_Unpin(void)
{
    uint32_t irq = __get_PRIMASK();
    __disable_irq();
    tx_block_idx = -1;
    __set_PRIMASK(irq);
}

uint8_t Buffer_CopyLatest(uint32_t after_generation, uint8_t *data, uint32_t *generation)
{
    uint8_t index;
    if ((data == NULL) ||
        (Buffer_PinLatest(after_generation, &index, generation) == 0U)) return 0U;
    memcpy(data, &can_block[index], CAN_BLOCK_TX_SIZE);
    Buffer_Unpin();
    return 1U;
}
