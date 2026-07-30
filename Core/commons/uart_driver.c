/**
  * @file    uart_driver.c
  * @author  Andriy Bratus <ambr75@gmail.com>
  * @brief   Source file for UART Rx/Tx DMA implementation.
  * @date    2026
  *
  * @copyright Copyright (c) 2026 Andriy Bratus <ambr75@gmail.com>
  *            All rights reserved.
  *
  * @attention
  * SPDX-License-Identifier: GPL-3.0-or-later
  */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "SEGGER_RTT.h"
#include "config.h"
#include "uart_driver.h"
#include "debug_log.h"

static UARTDriver_t* RegisteredUARTDrivers[MAX_UART_COUNT] = {0};

static bool Register_UARTDriver(UARTDriver_t* p_uart_driver) {
    for (uint8_t index = 0; index < MAX_UART_COUNT; ++index) {
        if ((RegisteredUARTDrivers[index] == NULL) || (RegisteredUARTDrivers[index]->_p_huart->Instance == p_uart_driver->_p_huart->Instance)) {
            RegisteredUARTDrivers[index] = p_uart_driver;
            return true;
        }
    }
    return false;
}

static UARTDriver_t* Find_UARTDriver(UART_HandleTypeDef *p_huart) {
    for (uint8_t index = 0; index < MAX_UART_COUNT; ++index) {
        if (RegisteredUARTDrivers[index]->_p_huart->Instance == p_huart->Instance) {
            return RegisteredUARTDrivers[index];
        }
    }
    return NULL;
}

static HAL_StatusTypeDef UART_Start_Receiving(UART_HandleTypeDef *p_huart, uint8_t *pData, uint16_t Size) {
    // Initial launch of circular reception via DMA with Idle detection
    HAL_StatusTypeDef result = HAL_UARTEx_ReceiveToIdle_DMA(p_huart, pData, Size);
    if (result == HAL_OK)
    {
        // __HAL_DMA_DISABLE_IT(p_huart->hdmarx, DMA_IT_HT); // It can be enabled now
    }
    return result;
}

static void UARTDriver_Init(UARTDriver_t *self, UART_HandleTypeDef *p_huart) {
    self->_p_huart = p_huart;
    self->rx_idle = true;
    self->_dma_old_pos = 0;
    self->_tx_completed = true;
    self->on_data_transmitted = NULL;
    self->on_data_received = NULL;
    RingBuffer_Ctor(&self->rx_fifo);

    UART_Start_Receiving(self->_p_huart, self->rx_fifo._data, self->rx_fifo.GetSize(&self->rx_fifo));
}

static void UARTDriver_RxEventCallback(UARTDriver_t* p_uart_driver, uint16_t dma_curr_pos) {
    // Modification by business logic before writing to FIFO
    p_uart_driver->rx_idle = false;
    int16_t last_chunk_size = (int16_t)(dma_curr_pos - p_uart_driver->_dma_old_pos);
    p_uart_driver->rx_fifo._head += last_chunk_size;
    p_uart_driver->rx_fifo._head %= p_uart_driver->rx_fifo.GetSize(&p_uart_driver->rx_fifo);
    if (last_chunk_size < 0) {
        LOG_WARN("UART Rx chunk size is negative!");
    }
    switch (p_uart_driver->_p_huart->RxEventType) {
        case HAL_UART_RXEVENT_TC:    /*!< RxEvent linked to Transfer Complete event */
            LOG_INFO("HAL_UART_RXEVENT_TC");
            break;             
        case HAL_UART_RXEVENT_HT:    /*!< RxEvent linked to Half Transfer event     */
            LOG_INFO("HAL_UART_RXEVENT_HT");
            break;
        case HAL_UART_RXEVENT_IDLE:
            p_uart_driver->rx_idle = true;
            LOG_INFO("HAL_UART_RXEVENT_IDLE");
            break;
    }
    if (p_uart_driver->on_data_received != NULL) {  
        p_uart_driver->on_data_received(p_uart_driver, p_uart_driver->rx_fifo._data + p_uart_driver->_dma_old_pos, last_chunk_size);
    }
    p_uart_driver->_dma_old_pos = dma_curr_pos % p_uart_driver->rx_fifo.GetSize(&p_uart_driver->rx_fifo);
}

static void UARTDriver_TxCpltCallback(UARTDriver_t* p_uart_driver) {
    p_uart_driver->_tx_completed = true;
    if (p_uart_driver->on_data_transmitted != NULL) {
        p_uart_driver->on_data_transmitted(p_uart_driver);
    }
}

static int32_t UARTDriver_transmit_data(UARTDriver_t *self, uint8_t *p_data, uint16_t len) {
    /* FIFO ➔ UART TX (DMA) */
    if (self->_tx_completed) {
        if (len > 0) {
            self->_tx_completed = false;
            LOG_INFO("UART transmitting %d bytes", len);
            if (HAL_UART_Transmit_DMA(self->_p_huart, p_data, len) == HAL_OK) {
                LOG_INFO("UART transmitting succeeded");
                return len;
            }
            else
            {
                self->_tx_completed = true; 
                LOG_ERR("UART transmitting failed!");
                return 0;
            }
        }
    }
    return 0;
}

static int32_t UARTDriver_transmit(UARTDriver_t *self, RingBuffer_t *p_tx_fifo) {
    /* FIFO ➔ UART TX (DMA) */
    int32_t result = 0;
    if (self->_tx_completed) {
        uint16_t fifo_buf_count = p_tx_fifo->GetCount(p_tx_fifo);
        if (fifo_buf_count > 0) {
            uint16_t send_len = p_tx_fifo->Read(p_tx_fifo, self->_tx_active_buf, fifo_buf_count);
            result = self->transmit_data(self, self->_tx_active_buf, send_len);
            if (result - send_len > 0) {
                p_tx_fifo->RollbackTail(p_tx_fifo, send_len - result);
            }
        }
    }
    return result;
}

// It's invoked from the HAL
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *p_huart, uint16_t Size) {
    LOG_INFO("HAL_UARTEx_RxEventCallback");
    UARTDriver_t* p_uart_driver = Find_UARTDriver(p_huart);
    if (p_uart_driver != NULL) {
        UARTDriver_RxEventCallback(p_uart_driver, Size);
    }
}

// It's invoked from the HAL
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *p_huart) {
    LOG_INFO("HAL_UART_TxCpltCallback");
    UARTDriver_t* p_uart_driver = Find_UARTDriver(p_huart);
    if (p_uart_driver != NULL) {
        UARTDriver_TxCpltCallback(p_uart_driver);
    }
}

void UARTDriver_Ctor(UARTDriver_t *self, UART_HandleTypeDef *p_huart) {
    if (self == NULL) return;
    if (!Register_UARTDriver(self)) return;

    self->init = UARTDriver_Init;
    self->transmit_data = UARTDriver_transmit_data;
    self->transmit = UARTDriver_transmit;
 
    self->init(self, p_huart);
}
