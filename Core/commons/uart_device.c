/**
  * @file    uart_device.c
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
#include <stdint.h>
#include "config.h"
#include "uart_device.h"
#include "debug_log.h"

static UARTDevice_t* RegisteredUARTDevices[MAX_UART_COUNT] = {0};

static bool Register_UARTDevice(UARTDevice_t* p_uart_device) {
    for (uint8_t index = 0; index < MAX_UART_COUNT; ++index) {
        if ((RegisteredUARTDevices[index] == NULL) || (RegisteredUARTDevices[index]->_p_huart->Instance == p_uart_device->_p_huart->Instance)) {
            RegisteredUARTDevices[index] = p_uart_device;
            return true;
        }
    }
    return false;
}

static UARTDevice_t* Find_UARTDevice(UART_HandleTypeDef *p_huart) {
    for (uint8_t index = 0; index < MAX_UART_COUNT; ++index) {
        if (RegisteredUARTDevices[index]->_p_huart->Instance == p_huart->Instance) {
            return RegisteredUARTDevices[index];
        }
    }
    return NULL;
}

static HAL_StatusTypeDef UART_Start_Receiving(UART_HandleTypeDef *p_huart, uint8_t *pData, uint16_t Size) {
    // Initial launch of circular reception via DMA with Idle detection
    HAL_StatusTypeDef result = HAL_UARTEx_ReceiveToIdle_DMA(p_huart, pData, Size);
    if (result == HAL_OK)
    {
        __HAL_DMA_DISABLE_IT(p_huart->hdmarx, DMA_IT_HT); 
    }
    return result;
}

void UARTDevice_Init(UARTDevice_t *self, UART_HandleTypeDef *p_huart) {
    self->_p_huart = p_huart;
    self->_old_pos = 0;
    self->_uart_tx_complete = true;
    RingBuffer_Ctor(&self->rx_fifo);

    UART_Start_Receiving(self->_p_huart, self->_rx_raw_buf, UART_RX_RAW_SIZE);
}

__weak uint16_t UARTDevice_on_data_received(UARTDevice_t *p_uart_device, uint8_t *p_data, uint16_t len) {
    return len;
}

__weak void UARTDevice_on_data_transmitted(UARTDevice_t *p_uart_device) {

}

static void UARTDevice_RxEventCallback(UARTDevice_t* p_uart_device, uint16_t Size) {
    uint16_t write_pos = Size;
    if (write_pos != p_uart_device->_old_pos) {
        uint16_t len = 0;
        uint8_t temp_proc_buf[UART_RX_RAW_SIZE];

        if (write_pos > p_uart_device->_old_pos) {
            len = write_pos - p_uart_device->_old_pos;
            memcpy(temp_proc_buf, &p_uart_device->_rx_raw_buf[p_uart_device->_old_pos], len);
        } else {
            len = UART_RX_RAW_SIZE - p_uart_device->_old_pos;
            memcpy(temp_proc_buf, &p_uart_device->_rx_raw_buf[p_uart_device->_old_pos], len);
            if (write_pos > 0) {
                memcpy(&temp_proc_buf[len], &p_uart_device->_rx_raw_buf[0], write_pos);
                len += write_pos;
            }
        }

        // Modification by business logic before writing to FIFO
        uint16_t modified_len = p_uart_device->on_data_received(p_uart_device, temp_proc_buf, len);
        if (modified_len > 0) {
            p_uart_device->rx_fifo.Write(&p_uart_device->rx_fifo, temp_proc_buf, modified_len);
        }
        p_uart_device->_old_pos = write_pos;
    }

    if (p_uart_device->_old_pos >= UART_RX_RAW_SIZE) {
        p_uart_device->_old_pos = 0;
    }
    
    UART_Start_Receiving(p_uart_device->_p_huart, p_uart_device->_rx_raw_buf, UART_RX_RAW_SIZE);
}

static void UARTDevice_TxCpltCallback(UARTDevice_t* p_uart_device) {
    p_uart_device->_uart_tx_complete = true;
    UARTDevice_on_data_transmitted(p_uart_device);
}

void UARTDevice_transmit(UARTDevice_t *p_uart_device, RingBuffer_t *p_uart_tx_fifo) {
    /* FIFO ➔ UART TX (DMA) */
    if (p_uart_device->_uart_tx_complete && p_uart_tx_fifo->GetCount(p_uart_tx_fifo) > 0) {
        uint16_t send_len = p_uart_tx_fifo->Read(p_uart_tx_fifo, p_uart_device->_tx_uart_active_buf, 
            p_uart_tx_fifo->GetSize(p_uart_tx_fifo));
        if (send_len > 0) {
            p_uart_device->_uart_tx_complete = false;
            LOG_INFO("UART transmitting %d bytes", send_len);
            if (HAL_UART_Transmit_DMA(p_uart_device->_p_huart, p_uart_device->_tx_uart_active_buf, send_len) == HAL_OK) {
                LOG_INFO("UART transmitting succeeded");
            }
            else
            {
                p_uart_device->_uart_tx_complete = true; 
                LOG_ERR("UART transmitting failed!");
            }
         }
    }
}

// It's invoked from the HAL
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *p_huart, uint16_t Size) {
    LOG_INFO("HAL_UARTEx_RxEventCallback");
    UARTDevice_t* p_uart_device = Find_UARTDevice(p_huart);
    if (p_uart_device != NULL) {
        UARTDevice_RxEventCallback(p_uart_device, Size);
    }
}

// It's invoked from the HAL
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *p_huart) {
    LOG_INFO("HAL_UART_TxCpltCallback");
    UARTDevice_t* p_uart_device = Find_UARTDevice(p_huart);
    if (p_uart_device != NULL) {
        UARTDevice_TxCpltCallback(p_uart_device);
    }
}

void UARTDevice_Ctor(UARTDevice_t *self, UART_HandleTypeDef *p_huart) {
    if (self == NULL) return;
    if (!Register_UARTDevice(self)) return;

    self->init = UARTDevice_Init;
    self->transmit = UARTDevice_transmit;
    self->on_data_received = UARTDevice_on_data_received;
 
    self->init(self, p_huart);
}
