/**
  * @file    usb_uart_bridge.h
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

#include "uart_driver.h"
#include "usb_driver.h"

// --- Public interface functions ---
typedef struct USBUARTBridge USBUARTBridge_t;

struct USBUARTBridge
{
    // private declarations
    USBDriver_t usb_driver;
    UARTDriver_t uart_driver;

    /**
    * @brief Initialize the bridge component.
    */
    void (*init)(USBUARTBridge_t* self, USBD_HandleTypeDef* p_husb, UART_HandleTypeDef* p_huart);
    /**
    * @brief Process USB <-> UART transmission.
    */
    void (*process)(USBUARTBridge_t* self);
};

void USBUARTBridge_Ctor(USBUARTBridge_t* self, USBD_HandleTypeDef* p_husb, UART_HandleTypeDef* p_huart);

#endif /* INC_USB_UART_BRIDGE_H_ */
