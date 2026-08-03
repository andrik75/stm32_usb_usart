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

// --- Implementation of public interface ---

/**
 * @brief Initialize the USB <-> UART bridge.
 */
static void USB_UART_Bridge_Init(USBUARTBridge_t* self, USBD_HandleTypeDef* p_husb, UART_HandleTypeDef* p_huart) {
    USBDriver_Ctor(&self->usb_driver, USB_FS, p_husb);
    UARTDriver_Ctor(&self->uart_driver, p_huart);
}

/**
 * @brief Background handler of the bridge. Must be called in main loop while(1).
 */
static void USB_UART_Bridge_Process(USBUARTBridge_t* self) {
    self->uart_driver.transmit(&self->uart_driver, &self->usb_driver.rx_fifo);
    self->usb_driver.resume_rx(&self->usb_driver);
    self->usb_driver.transmit(&self->usb_driver, &self->uart_driver.rx_fifo);
}

void USBUARTBridge_Ctor(USBUARTBridge_t* self, USBD_HandleTypeDef* p_husb, UART_HandleTypeDef* p_huart) {
    self->init = USB_UART_Bridge_Init;
    self->process = USB_UART_Bridge_Process;
    self->init(self, p_husb, p_huart);
}
