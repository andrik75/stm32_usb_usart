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


// Forward declaration of the UART_HandleTypeDef structure

#include "ring_buffer.h"
#include "i2c_driver.h"
#include "uart_driver.h"

// --- Public interface functions ---
typedef struct I2CUARTBridge I2CUARTBridge_t;

struct I2CUARTBridge
{
    // private declarations
    I2CDriver_t i2c_driver;
    UARTDriver_t uart_driver;

    RingBuffer_t uart_tx_fifo;
    RingBuffer_t i2c_tx_fifo;

    /**
    * @brief Initialize the bridge component.
    */
    void (*init)(I2CUARTBridge_t* self, I2C_HandleTypeDef* p_hi2c, UART_HandleTypeDef* p_huart);
    /**
    * @brief Process USB <-> UART transmission.
    */
    void (*process)(I2CUARTBridge_t* self, bool is_master);
    /**
    * @brief USB <-> UART transmission event handlers
    */
    void (*on_uart_data_received)(I2CUARTBridge_t* self, uint8_t *p_data, const uint16_t len);
    void (*on_uart_data_transmitted)(I2CUARTBridge_t* self);
    void (*on_i2c_data_received)(I2CUARTBridge_t* self, uint8_t *p_data, const uint16_t len);
    void (*on_i2c_data_transmitted)(I2CUARTBridge_t* self);
};

void I2CUARTBridge_Ctor(I2CUARTBridge_t* self, I2C_HandleTypeDef* p_hi2c, UART_HandleTypeDef* p_huart);

#endif /* INC_I2C_UART_BRIDGE_H_ */
