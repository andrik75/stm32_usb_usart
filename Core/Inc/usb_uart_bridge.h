/**
  * @file    usb_uart_bridge.c
  * @author  Andriy Bratus <ambr75@gmail.com>
  * @brief   Header file for USB <-> UART transparent transmission implementation.
  * @date    2026
  *
  * @copyright Copyright (c) 2026 Andriy Bratus <ambr75@gmail.com>
  *            All rights reserved.
  *
  * @attention
  * SPDX-License-Identifier: GPL-3.0-or-later
  */
#ifndef INC_USB_UART_BRIDGE_H_
#define INC_USB_UART_BRIDGE_H_

// Forward declaration of the UART_HandleTypeDef structure
typedef struct __UART_HandleTypeDef UART_HandleTypeDef;

#include "uart_device.h"
#include "usb_rx_tx.h"

// --- Public interface functions ---

/**
 * @brief Background handler of the bridge. Must be called in main loop while(1).
 */
void USB_UART_Bridge_Process(UARTDevice_t *p_uart_device, USBDevice_t *p_usb_device);

#endif /* INC_USB_UART_BRIDGE_H_ */
