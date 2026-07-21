/**
  * @file    uart_rx_tx.h
  * @author  Andriy Bratus <ambr75@gmail.com>
  * @brief   Header file for UART Rx/Tx DMA implementation.
  * @date    2026
  *
  * @copyright Copyright (c) 2026 Andriy Bratus <ambr75@gmail.com>
  *            All rights reserved.
  *
  * @attention
  * SPDX-License-Identifier: GPL-3.0-or-later
  */
#ifndef INC_UART_RX_TX_H_
#define INC_UART_RX_TX_H_

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
void UART_transmit(RingBuffer_t *p_uart_tx_fifo);

#endif /* INC_UART_RX_TX_H_ */
