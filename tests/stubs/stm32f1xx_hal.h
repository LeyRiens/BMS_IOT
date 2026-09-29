#ifndef STM32F1XX_HAL_H
#define STM32F1XX_HAL_H
#include <stdint.h>
#include <stddef.h>
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct { uint32_t Mode; } DMA_InitTypeDef;
typedef struct { DMA_InitTypeDef Init; uint16_t remaining; } DMA_HandleTypeDef;
typedef struct { DMA_HandleTypeDef *hdmarx; uint32_t RxState; uint32_t ErrorCode; } UART_HandleTypeDef;
typedef struct { int unused; } GPIO_TypeDef;
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
#define DMA_NORMAL 0U
#define DMA_CIRCULAR 1U
#define HAL_UART_STATE_READY 0U
#define HAL_UART_STATE_BUSY_RX 1U
#define HAL_UART_ERROR_NONE 0U
#define HAL_UART_ERROR_ORE 8U
#define USART1_IRQn 0
#define __HAL_DMA_GET_COUNTER(dma) ((dma)->remaining)
static inline uint32_t __get_PRIMASK(void) { return 0U; }
static inline void __disable_irq(void) {}
static inline void __set_PRIMASK(uint32_t value) { (void)value; }
uint32_t HAL_GetTick(void);
void HAL_NVIC_SetPriority(int irq, uint32_t priority, uint32_t subpriority);
void HAL_NVIC_EnableIRQ(int irq);
HAL_StatusTypeDef HAL_UART_Receive_DMA(UART_HandleTypeDef *, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *);
#endif
