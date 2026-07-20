#ifndef INC_BRIDGE_UART_H_
#define INC_BRIDGE_UART_H_

#include "stm32f1xx_hal.h"
#include "ring_buffer.h"

#define UART_RX_RAW_SIZE  256   // Size of the raw DMA buffer for UART

/**
 * @brief Initialize UART interface component for the bridge.
 */
void UART_RX_TX_Init(UART_HandleTypeDef *huart);

/**
 * @brief Process UART transmission from FIFO.
 */
void UART_Process_TX(RingBuffer_t *p_uart_tx_fifo);

#endif /* INC_BRIDGE_UART_H_ */
