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
extern RingBuffer_t usb_rx_fifo;

// --- Implementation of public interface ---

void USB_UART_Bridge_Init(UARTDevice_t *p_uart_device, UART_HandleTypeDef *p_huart) {
    RingBuffer_Ctor(&usb_rx_fifo); // ring buffer to receive data from USB and transmit them via UART
 
    USB_RX_TX_Init();
    UARTDevice_Ctor(p_uart_device, p_huart);
}

void USB_UART_Bridge_Process(UARTDevice_t *p_uart_device) {
    p_uart_device->transmit(p_uart_device, &usb_rx_fifo);
    USB_Resume_RX();
    USB_transmit(&p_uart_device->rx_fifo);
}
