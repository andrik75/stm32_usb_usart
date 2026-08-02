/**
  * @file    i2c_uart_bridge.h
  * @author  Andriy Bratus <ambr75@gmail.com>
  * @brief   Header file for I2C <-> UART transparent transmission implementation.
  * @date    2026
  *
  * @copyright Copyright (c) 2026 Andriy Bratus <ambr75@gmail.com>
  *            All rights reserved.
  *
  * @attention
  * SPDX-License-Identifier: GPL-3.0-or-later
  */
#ifndef INC_I2C_UART_BRIDGE_H_
#define INC_I2C_UART_BRIDGE_H_

#include <stdbool.h>

/**
 * @brief Initialize the UART <-> I2C bridge hardware drivers and FIFOs.
 */
void I2C_UART_Bridge_Init(UART_HandleTypeDef* p_uart_handle, I2C_HandleTypeDef* p_usb_handle);

/**
 * @brief Process data transfer between UART and I2C interfaces.
 * @param is_master Set to true if the current board operates as I2C Master, false for Slave.
 */
void I2C_UART_Bridge_Process(bool is_master);

#endif /* INC_I2C_UART_BRIDGE_H_ */
