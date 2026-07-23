/**
  * @file    uart_driver.h
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
#ifndef INC_UART_DRIVER_H_
#define INC_UART_DRIVER_H_

#include <stdbool.h>
#include <stdint.h>
#include "ring_buffer.h"

#define UART_RX_RAW_SIZE  (256)   // Size of the raw DMA buffer for UART

// Forward declaration of the UART_HandleTypeDef structure
typedef struct __UART_HandleTypeDef UART_HandleTypeDef;

typedef struct UARTDevice UARTDevice_t;

struct UARTDevice
{
    // private declarations
    UART_HandleTypeDef *_p_huart;
    uint8_t _rx_raw_buf[UART_RX_RAW_SIZE];
    uint32_t _old_pos;

    uint8_t _tx_uart_active_buf[RING_BUFFER_SIZE];
    volatile bool _uart_tx_complete;

    // public declarations
    RingBuffer_t rx_fifo;
    /**
    * @brief Initialize UART interface component.
    */
    void (*init)(UARTDevice_t *self, UART_HandleTypeDef *p_huart);
    /**
    * @brief Process UART transmission from the buffer.
    */
    void (*transmit)(UARTDevice_t *self, RingBuffer_t *p_tx_fifo);
    void (*on_data_transmitted)(UARTDevice_t *self);
    uint16_t (*on_data_received)(UARTDevice_t *self, uint8_t *p_data, uint16_t len);
};

void UARTDevice_Ctor(UARTDevice_t *self, UART_HandleTypeDef *p_huart);

#endif /* INC_UART_DRIVER_H_ */
