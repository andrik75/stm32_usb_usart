/**
  * @file    i2c_driver.c
  * @author  Andriy Bratus <ambr75@gmail.com>
  * @brief   Source file for I2C DMA implementation.
  * @date    2026
  *
  * @copyright Copyright (c) 2026 Andriy Bratus <ambr75@gmail.com>
  *            All rights reserved.
  *
  * @attention
  * SPDX-License-Identifier: GPL-3.0-or-later
  */

#include "config.h"
#include "i2c_driver.h"
#include "debug_log.h"
#include <stdint.h>

static I2CDriver_t* RegisteredI2CDrivers[MAX_I2C_COUNT] = {0};

static bool Register_I2CDriver(I2CDriver_t* p_i2c_driver) {
    for (uint8_t index = 0; index < MAX_I2C_COUNT; ++index) {
        if ((RegisteredI2CDrivers[index] == NULL) || (RegisteredI2CDrivers[index]->_p_hi2c->Instance == p_i2c_driver->_p_hi2c->Instance)) {
            RegisteredI2CDrivers[index] = p_i2c_driver;
            return true;
        }
    }
    return false;
}

static I2CDriver_t* Find_I2CDriver(I2C_HandleTypeDef *p_hi2c) {
    for (uint8_t index = 0; index < MAX_I2C_COUNT; ++index) {
        if (RegisteredI2CDrivers[index]->_p_hi2c->Instance == p_hi2c->Instance) {
            return RegisteredI2CDrivers[index];
        }
    }
    return NULL;
}

static HAL_StatusTypeDef I2C_Start_Receiving(I2C_HandleTypeDef *p_hi2c, uint8_t *pData, uint16_t Size) {
    // Initial launch of slave reception via DMA
    HAL_StatusTypeDef result = HAL_I2C_Slave_Receive_DMA(p_hi2c, pData, Size);
    if (result == HAL_OK)
    {
        // DMA initialization checks can be performed here if needed
    }
    return result;
}

static void I2CDriver_Init(I2CDriver_t *self, I2C_HandleTypeDef *p_hi2c) {
    self->p_owner = NULL;
    self->_p_hi2c = p_hi2c;
    self->rx_idle = true;
    self->_tx_completed = true;
    self->on_data_transmitted = NULL;
    self->on_data_received = NULL;
    RingBuffer_Ctor(&self->rx_fifo);

    if (!IS_I2C_MASTER) {
        I2C_Start_Receiving(self->_p_hi2c, self->rx_fifo._data, self->rx_fifo.GetCapacity(&self->rx_fifo));
    }
}

static void I2CDriver_RxEventCallback(I2CDriver_t* p_i2c_driver, uint16_t dma_curr_pos) {
    // Modification by business logic before writing to FIFO
    p_i2c_driver->rx_idle = false;
    int32_t last_chunk_size = (int32_t)dma_curr_pos - (int32_t)p_i2c_driver->rx_fifo._head;
    if (last_chunk_size >= 0) {
        if (p_i2c_driver->on_data_received != NULL) {  
            p_i2c_driver->on_data_received(p_i2c_driver, p_i2c_driver->rx_fifo._data + p_i2c_driver->rx_fifo._head, (uint16_t)last_chunk_size);
        }
    } else {
        LOG_WARN("I2C Rx chunk size is negative!");
    }
    p_i2c_driver->rx_fifo.SetHead(&p_i2c_driver->rx_fifo, dma_curr_pos);

    p_i2c_driver->rx_idle = true;
    if (!IS_I2C_MASTER) {
        I2C_Start_Receiving(p_i2c_driver->_p_hi2c, p_i2c_driver->rx_fifo._data, p_i2c_driver->rx_fifo.GetCapacity(&p_i2c_driver->rx_fifo));
    }
    LOG_INFO("I2C RxEvent complete");
}

static void I2CDriver_TxCpltCallback(I2CDriver_t* p_i2c_driver) {
    p_i2c_driver->_tx_completed = true;
    if (p_i2c_driver->on_data_transmitted != NULL) {
        p_i2c_driver->on_data_transmitted(p_i2c_driver);
    }
}

static int32_t I2CDriver_transmit_data(I2CDriver_t *self, uint8_t *p_data, uint16_t len, uint16_t target_address) {
    /* FIFO ➔ I2C TX (DMA) */
    if (self->_tx_completed) {
        if (len > 0) {
            self->_tx_completed = false;
            LOG_INFO("I2C transmitting %d bytes to 0x%02X", len, target_address);
            if (HAL_I2C_Master_Transmit_DMA(self->_p_hi2c, target_address, p_data, len) == HAL_OK) {
                LOG_INFO("I2C transmitting succeeded");
                return len;
            }
            else
            {
                self->_tx_completed = true; 
                LOG_ERR("I2C transmitting failed!");
                return 0;
            }
        }
    }
    return 0;
}

static int32_t I2CDriver_transmit(I2CDriver_t *self, RingBuffer_t *p_tx_fifo, uint16_t target_address) {
    /* FIFO ➔ I2C TX (DMA) */
    int32_t result = 0;
    if (self->_tx_completed) {
        uint16_t fifo_buf_count = p_tx_fifo->GetCount(p_tx_fifo);
        if (fifo_buf_count > 0) {
            uint16_t send_len = p_tx_fifo->Read(p_tx_fifo, self->_tx_active_buf, fifo_buf_count);
            result = self->transmit_data(self, self->_tx_active_buf, send_len, target_address);
            if ((int32_t)result - (int32_t)send_len < 0) {
                p_tx_fifo->RollbackTail(p_tx_fifo, send_len - result);
            }
        }
    }
    return result;
}

bool I2CDriver_IsDeviceReady(I2CDriver_t *self, uint16_t device_address) {
    return HAL_I2C_IsDeviceReady(self->_p_hi2c, device_address, 5, 1000) == HAL_OK;
}

// It's invoked from the HAL
void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *p_hi2c) {
    LOG_INFO("HAL_I2C_SlaveRxCpltCallback");
    I2CDriver_t* p_i2c_driver = Find_I2CDriver(p_hi2c);
    if (p_i2c_driver != NULL) {
        I2CDriver_RxEventCallback(p_i2c_driver, p_i2c_driver->rx_fifo.GetCapacity(&p_i2c_driver->rx_fifo));
    }
}

// It's invoked from the HAL
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *p_hi2c) {
    LOG_INFO("HAL_I2C_MasterTxCpltCallback");
    I2CDriver_t* p_i2c_driver = Find_I2CDriver(p_hi2c);
    if (p_i2c_driver != NULL) {
        I2CDriver_TxCpltCallback(p_i2c_driver);
    }
}

// It's invoked from the HAL
void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *p_hi2c) {
    LOG_INFO("HAL_I2C_SlaveTxCpltCallback");
    I2CDriver_t* p_i2c_driver = Find_I2CDriver(p_hi2c);
    if (p_i2c_driver != NULL) {
        I2CDriver_TxCpltCallback(p_i2c_driver);
    }
}

void I2CDriver_Ctor(I2CDriver_t *self, I2C_HandleTypeDef *p_hi2c) {
    if (self == NULL) return;

    self->init = I2CDriver_Init;
    self->transmit_data = I2CDriver_transmit_data;
    self->transmit = I2CDriver_transmit;
    self->is_device_ready = I2CDriver_IsDeviceReady;

    self->init(self, p_hi2c);
    Register_I2CDriver(self);
}
