/**
  * @file    i2c_driver.h
  * @author  Andriy Bratus <ambr75@gmail.com>
  * @brief   Header file for I2C DMA implementation.
  * @date    2026
  *
  * @copyright Copyright (c) 2026 Andriy Bratus <ambr75@gmail.com>
  *            All rights reserved.
  *
  * @attention
  * SPDX-License-Identifier: GPL-3.0-or-later
  */

#ifndef INC_I2C_DRIVER_H_
#define INC_I2C_DRIVER_H_

#include <stdbool.h>
#include <stdint.h>
#include "config.h"
#include "ring_buffer.h"

#define I2C_RX_RAW_SIZE  (256)   // Size of the raw DMA buffer for I2C

typedef struct I2CDriver I2CDriver_t;

struct I2CDriver
{
    // private declarations
    I2C_HandleTypeDef *_p_hi2c;

    uint8_t _tx_active_buf[RING_BUFFER_SIZE];
    volatile bool _tx_completed;

    // public declarations
    RingBuffer_t rx_fifo;
    volatile bool rx_idle;
    /**
    * @brief Initialize I2C interface component.
    */
    void (*init)(I2CDriver_t *self, I2C_HandleTypeDef *p_hi2c);
    /**
    * @brief Process I2C transmission from the buffer.
    */
    int32_t (*transmit_data)(I2CDriver_t *self, uint8_t *p_data, uint16_t len, uint16_t target_address);
    int32_t (*transmit)(I2CDriver_t *self, RingBuffer_t *p_tx_fifo, uint16_t target_address);
    void (*on_data_transmitted)(I2CDriver_t *self);
    void (*on_data_received)(I2CDriver_t *self, uint8_t *p_data, const uint16_t len);
};

void I2CDriver_Ctor(I2CDriver_t *self, I2C_HandleTypeDef *p_hi2c);

#endif /* INC_I2C_DRIVER_H_ */
