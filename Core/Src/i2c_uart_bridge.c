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
#include "debug_log.h"
#include "i2c_uart_bridge.h"

#define SLAVE_I2C_ADDR (0x58 << 1) // 7-bit address 0x58 shifted left for STM32 HAL (0xB0)

// -------------------------------------------------------------------
// UART Callbacks
// -------------------------------------------------------------------

/* Triggered when data is received from PC via UART -> queue it for I2C transmission */
__weak void On_I2C_UART_Bridge_UART_Data_Received(I2CUARTBridge_t* self, uint8_t *p_data, const uint16_t len) {
    // Copy incoming UART bytes into I2C TX buffer
    self->i2c_tx_fifo.Write(&self->i2c_tx_fifo, p_data, len);
}

/* Triggered when UART DMA transmission completes */
__weak void On_I2C_UART_Bridge_UART_Data_Transmitted(I2CUARTBridge_t* self) {
    // Current UART TX chunk completed
}

// -------------------------------------------------------------------
// I2C Callbacks
// -------------------------------------------------------------------

/* Triggered when data is received via I2C -> queue it for UART transmission to PC */
__weak void On_I2C_UART_Bridge_I2C_Data_Received(I2CUARTBridge_t* self, uint8_t *p_data, const uint16_t len) {
    // Copy incoming I2C bytes into UART TX buffer
    self->uart_tx_fifo.Write(&self->uart_tx_fifo, p_data, len);
}

/* Triggered when I2C DMA transmission completes */
__weak void On_I2C_UART_Bridge_I2C_Data_Transmitted(I2CUARTBridge_t* self) {
    // Current I2C TX chunk completed
}

/* private handlers*/

// -------------------------------------------------------------------
// UART Driver Callbacks
// -------------------------------------------------------------------

/* Triggered when data is received from PC via UART -> queue it for I2C transmission */
static void On_UART_Data_Received(UARTDriver_t* self, uint8_t *p_data, const uint16_t len) {
    // Passing the parameters into the bridge appropriate handler
    if (self->p_owner != NULL) {
        I2CUARTBridge_t *bridge = (I2CUARTBridge_t*)self->p_owner;
        if (bridge->on_uart_data_received != NULL) {
            bridge->on_uart_data_received(bridge, p_data, len);
        }
    }
}

/* Triggered when UART DMA transmission completes */
static void On_UART_Data_Transmitted(UARTDriver_t* self) {
    // Passing the parameters into the bridge appropriate handler
    if (self->p_owner != NULL) {
        I2CUARTBridge_t *bridge = (I2CUARTBridge_t*)self->p_owner;
        if (bridge->on_uart_data_transmitted != NULL) {
            bridge->on_uart_data_transmitted(bridge);
        }
    }
}

// -------------------------------------------------------------------
// I2C Driver Callbacks
// -------------------------------------------------------------------

/* Triggered when data is received via I2C -> queue it for UART transmission to PC */
static void On_I2C_Data_Received(I2CDriver_t* self, uint8_t *p_data, const uint16_t len) {
    // Passing the parameters into the bridge appropriate handler
    if (self->p_owner != NULL) {
        I2CUARTBridge_t *bridge = (I2CUARTBridge_t*)self->p_owner;
        if (bridge->on_i2c_data_received != NULL) {
            bridge->on_i2c_data_received(bridge, p_data, len);
        }
    }
}

/* Triggered when I2C DMA transmission completes */
static void On_I2C_Data_Transmitted(I2CDriver_t* self) {
    // Passing the parameters into the bridge appropriate handler
    if (self->p_owner != NULL) {
        I2CUARTBridge_t *bridge = (I2CUARTBridge_t*)self->p_owner;
        if (bridge->on_i2c_data_transmitted != NULL) {
            bridge->on_i2c_data_transmitted(bridge);
        }
    }
}

// -------------------------------------------------------------------
// Bridge Logic
// -------------------------------------------------------------------

// --- Implementation of public interface ---

/**
 * @brief Initialize the I2C <-> UART bridge.
 */
static void I2C_UART_Bridge_Init(I2CUARTBridge_t* self, I2C_HandleTypeDef* p_hi2c, UART_HandleTypeDef* p_huart) {
    // Construct ring buffers for transmit pipelines
    RingBuffer_Ctor(&self->uart_tx_fifo);
    RingBuffer_Ctor(&self->i2c_tx_fifo);

    // Initialize UART driver instance
    UARTDriver_Ctor(&self->uart_driver, p_huart);
    self->uart_driver.on_data_received = On_UART_Data_Received;
    self->uart_driver.on_data_transmitted = On_UART_Data_Transmitted;
    self->uart_driver.p_owner = (void*)self;

    // Initialize I2C driver instance (automatically enters Slave Receive/Listen mode)
    I2CDriver_Ctor(&self->i2c_driver, p_hi2c);
    self->i2c_driver.on_data_received = On_I2C_Data_Received;
    self->i2c_driver.on_data_transmitted = On_I2C_Data_Transmitted;
    self->i2c_driver.p_owner = (void*)self;

    // Initialize bridge callback handlers
    self->on_uart_data_received = On_I2C_UART_Bridge_UART_Data_Received;
    self->on_uart_data_transmitted = On_I2C_UART_Bridge_UART_Data_Transmitted;
    self->on_i2c_data_received = On_I2C_UART_Bridge_I2C_Data_Received;
    self->on_i2c_data_transmitted = On_I2C_UART_Bridge_I2C_Data_Transmitted;
    
    LOG_INFO("I2C <-> UART Bridge Initialized");
}

/**
 * @brief Background handler of the bridge. Must be called in main loop while(1).
 */
 /* It is called from the infinite main loop or from a FreeRTOS task */
static void I2C_UART_Bridge_Process(I2CUARTBridge_t* self, bool is_master) {
   // 1. Flush data from UART TX FIFO out to PC via UART DMA
    if (self->uart_tx_fifo.GetCount(&self->uart_tx_fifo) > 0) {
        self->uart_driver.transmit(&self->uart_driver, &self->uart_tx_fifo);
    }

    // 2. Flush data from I2C TX FIFO out to the I2C bus
    if (self->i2c_tx_fifo.GetCount(&self->i2c_tx_fifo) > 0) {
        if (is_master) {
            // Master initiates I2C DMA transmit to Target Slave (0x58)
            self->i2c_driver.transmit(&self->i2c_driver, &self->i2c_tx_fifo, SLAVE_I2C_ADDR);
        } else {
            // Slave TX handling can be processed here if Read Request from Master occurs
        }
    }
}

void I2CUARTBridge_Ctor(I2CUARTBridge_t* self, I2C_HandleTypeDef* p_hi2c, UART_HandleTypeDef* p_huart) {
    self->init = I2C_UART_Bridge_Init;
    self->process = I2C_UART_Bridge_Process;
    self->init(self, p_hi2c, p_huart);
}
