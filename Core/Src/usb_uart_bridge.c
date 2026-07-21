/**
  * @file    usb_uart_bridge.c
  * @author  Andriy Bratus <ambr75@gmail.com>
  * @brief   Source file for USB <-> UART transparent transmission implementation.
  * @date    2026
  *
  * @copyright Copyright (c) 2026 Andriy Bratus <ambr75@gmail.com>
  *            All rights reserved.
  *
  * @attention
  * SPDX-License-Identifier: GPL-3.0-or-later
  */
#include <stddef.h>
#include <stdint.h>
#include "usb_uart_bridge.h"
#include "ring_buffer.h"
#include "uart_rx_tx.h"
#include "usb_rx_tx.h"

// Shared global instances
extern RingBuffer_t uart_rx_fifo;
extern RingBuffer_t usb_rx_fifo;

// --- Implementation of public interface ---

void USB_UART_Bridge_Init(UART_HandleTypeDef *huart) {
    RingBuffer_Ctor(&usb_rx_fifo); // ring buffer to receive data from USB and transmit them via UART
    RingBuffer_Ctor(&uart_rx_fifo); // ring buffer to receive data from UART and transmit them via USB

    USB_RX_TX_Init();
    UART_RX_TX_Init(huart);
}

void USB_UART_Bridge_Process(void) {
    UART_transmit(&usb_rx_fifo);
    USB_Resume_RX();
    USB_transmit(&uart_rx_fifo);
}
