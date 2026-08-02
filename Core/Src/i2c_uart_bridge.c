/**
  * @file    i2c_uart_bridge.c
  * @author  Andriy Bratus <ambr75@gmail.com>
  * @brief   Source file for I2C <-> UART transparent transmission implementation.
  * @date    2026
  *
  * @copyright Copyright (c) 2026 Andriy Bratus <ambr75@gmail.com>
  *            All rights reserved.
  *
  * @attention
  * SPDX-License-Identifier: GPL-3.0-or-later
  */
#include "i2c_driver.h"
#include "uart_driver.h"
#include "ring_buffer.h"
#include "debug_log.h"
#include "i2c_uart_bridge.h"

#define SLAVE_I2C_ADDR (0x58 << 1) // 7-bit address 0x58 shifted left for STM32 HAL (0xB0)

// Driver instances
static UARTDriver_t uart_drv;
static I2CDriver_t  i2c_drv;

// Intermediate FIFOs for asynchronous TX transfers
static RingBuffer_t uart_tx_fifo;
static RingBuffer_t i2c_tx_fifo;

// -------------------------------------------------------------------
// UART Callbacks
// -------------------------------------------------------------------

/* Triggered when data is received from PC via UART -> queue it for I2C transmission */
static void On_UART_Data_Received(UARTDriver_t *self, uint8_t *p_data, const uint16_t len) {
    // Copy incoming UART bytes into I2C TX buffer
    i2c_tx_fifo.Write(&i2c_tx_fifo, p_data, len);
}

/* Triggered when UART DMA transmission completes */
static void On_UART_Data_Transmitted(UARTDriver_t *self) {
    // Current UART TX chunk completed
}

// -------------------------------------------------------------------
// I2C Callbacks
// -------------------------------------------------------------------

/* Triggered when data is received via I2C -> queue it for UART transmission to PC */
static void On_I2C_Data_Received(I2CDriver_t *self, uint8_t *p_data, const uint16_t len) {
    // Copy incoming I2C bytes into UART TX buffer
    uart_tx_fifo.Write(&uart_tx_fifo, p_data, len);
}

/* Triggered when I2C DMA transmission completes */
static void On_I2C_Data_Transmitted(I2CDriver_t *self) {
    // Current I2C TX chunk completed
}

// -------------------------------------------------------------------
// Bridge Logic
// -------------------------------------------------------------------

void I2C_UART_Bridge_Init(UART_HandleTypeDef* p_uart_handle, I2C_HandleTypeDef* p_usb_handle) {
    // Construct ring buffers for transmit pipelines
    RingBuffer_Ctor(&uart_tx_fifo);
    RingBuffer_Ctor(&i2c_tx_fifo);

    // Initialize UART driver instance
    UARTDriver_Ctor(&uart_drv, p_uart_handle);
    uart_drv.on_data_received = On_UART_Data_Received;
    uart_drv.on_data_transmitted = On_UART_Data_Transmitted;

    // Initialize I2C driver instance (automatically enters Slave Receive/Listen mode)
    I2CDriver_Ctor(&i2c_drv, p_usb_handle);
    i2c_drv.on_data_received = On_I2C_Data_Received;
    i2c_drv.on_data_transmitted = On_I2C_Data_Transmitted;

    LOG_INFO("UART <-> I2C Bridge Initialized");
}

/* It is called from the infinite main loop or from a FreeRTOS task */
void I2C_UART_Bridge_Process(bool is_master) {
    // 1. Flush data from UART TX FIFO out to PC via UART DMA
    if (uart_tx_fifo.GetCount(&uart_tx_fifo) > 0) {
        uart_drv.transmit(&uart_drv, &uart_tx_fifo);
    }

    // 2. Flush data from I2C TX FIFO out to the I2C bus
    if (i2c_tx_fifo.GetCount(&i2c_tx_fifo) > 0) {
        if (is_master) {
            // Master initiates I2C DMA transmit to Target Slave (0x58)
            i2c_drv.transmit(&i2c_drv, &i2c_tx_fifo, SLAVE_I2C_ADDR);
        } else {
            // Slave TX handling can be processed here if Read Request from Master occurs
        }
    }
}
