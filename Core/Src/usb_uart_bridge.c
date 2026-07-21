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

// --- USER CODE SECTION (DATA MODIFICATION BUSINESS LOGIC) ---

/**
 * @brief Data comes from USB from PC here before being sent to UART.
 *        You can modify the 'data' array on the fly.
 * @return New data length (if changed). 0 — discard packet.
 */
__weak uint16_t USB_UART_Bridge_OnUSBDataReceived(uint8_t *data, uint16_t len) {
    // By default, just pass data further unchanged
    return len;
}

uint16_t USB_on_data_received(uint8_t *data, uint16_t len) {
    return USB_UART_Bridge_OnUSBDataReceived(data, (uint16_t)len);
 }

/**
 * @brief Data comes from UART here before being sent to USB to PC.
 */
__weak uint16_t USB_UART_Bridge_OnUARTDataReceived(uint8_t *data, uint16_t len) {
    // By default, just pass data further unchanged
    return len;
}

uint16_t UART_on_data_received(uint8_t *data, uint16_t len) {
    return USB_UART_Bridge_OnUARTDataReceived(data, (uint16_t)len);
 }

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
