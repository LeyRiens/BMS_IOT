#ifndef BUFFER_H
#define BUFFER_H
#include <stdint.h>
#include <config.h>

#define CAN_NO_BLOCK 0xFFU

typedef struct { uint32_t id; uint8_t data[CAN_FRAME_DATA_SIZE]; } CAN_Frame_t;
typedef struct { CAN_Frame_t frame[CAN_FRAME_COUNT]; uint16_t block_seq; } CAN_Block_t;

extern CAN_Block_t can_block[CAN_BUFFER_COUNT];
extern volatile uint8_t write_idx;
extern volatile uint8_t latest_ready_idx;
extern volatile uint16_t global_block_seq;
extern volatile int8_t tx_block_idx;
extern volatile uint32_t can_invalid_block_count;
extern volatile uint32_t can_tx_collision_count;

void Buffer_Init(void);
void Buffer_ReceiveFrame(uint32_t id, const uint8_t data[8]);
uint32_t Buffer_GetGeneration(void);
uint8_t Buffer_HasNew(uint32_t after_generation);
uint8_t Buffer_PinLatest(uint32_t after_generation, uint8_t *index, uint32_t *generation);
void Buffer_Unpin(void);
/* Copy under a short pin; acquisition is free to overwrite during UART transmission. */
uint8_t Buffer_CopyLatest(uint32_t after_generation, uint8_t *data, uint32_t *generation);
#endif
