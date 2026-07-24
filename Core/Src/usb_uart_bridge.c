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

// Shared global instances
extern RingBuffer_t usb_rx_fifo;

// --- Implementation of public interface ---

void USB_UART_Bridge_Process(UARTDriver_t *p_uart_driver, USBDriver_t *p_usb_driver) {
    p_uart_driver->transmit(p_uart_driver, &p_usb_driver->rx_fifo);
    p_usb_driver->resume_rx(p_usb_driver);
    p_usb_driver->transmit(p_usb_driver, &p_uart_driver->rx_fifo);
}
