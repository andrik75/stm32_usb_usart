/**
  * @file    uart_rx_tx.c
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
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "config.h"
#include "uart_rx_tx.h"
#include "debug_log.h"

RingBuffer_t uart_rx_fifo;
static UART_HandleTypeDef *p_huart = NULL;
static uint8_t rx_raw_buf[UART_RX_RAW_SIZE];
static uint32_t old_pos = 0;

static uint8_t tx_uart_active_buf[RING_BUFFER_SIZE];
static volatile bool uart_tx_complete = true;

static HAL_StatusTypeDef UART_Start_Receiving(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size)
{
    // Initial launch of circular reception via DMA with Idle detection
    HAL_StatusTypeDef result = HAL_UARTEx_ReceiveToIdle_DMA(p_huart, rx_raw_buf, UART_RX_RAW_SIZE);
    if (result == HAL_OK)
    {
        __HAL_DMA_DISABLE_IT(p_huart->hdmarx, DMA_IT_HT); 
    }
    return result;
}

void UART_RX_TX_Init(UART_HandleTypeDef *huart) {
    p_huart = huart;
    old_pos = 0;
    uart_tx_complete = true;

    UART_Start_Receiving(p_huart, rx_raw_buf, UART_RX_RAW_SIZE);
}

__weak uint16_t UART_on_data_received(uint8_t *data, uint16_t len)
{
    return len;
}

__weak void UART_on_data_transmitted(UART_HandleTypeDef *huart)
{

}

static void UART_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    if (p_huart != NULL && huart->Instance == p_huart->Instance) {
        uint16_t write_pos = Size;
        if (write_pos != old_pos) {
            uint16_t len = 0;
            uint8_t temp_proc_buf[UART_RX_RAW_SIZE];

            if (write_pos > old_pos) {
                len = write_pos - old_pos;
                memcpy(temp_proc_buf, &rx_raw_buf[old_pos], len);
            } else {
                len = UART_RX_RAW_SIZE - old_pos;
                memcpy(temp_proc_buf, &rx_raw_buf[old_pos], len);
                if (write_pos > 0) {
                    memcpy(&temp_proc_buf[len], &rx_raw_buf[0], write_pos);
                    len += write_pos;
                }
            }

            // Modification by business logic before writing to FIFO
            uint16_t modified_len = UART_on_data_received(temp_proc_buf, len);
            if (modified_len > 0) {
                uart_rx_fifo.Write(&uart_rx_fifo, temp_proc_buf, modified_len);
            }
            old_pos = write_pos;
        }

        if (old_pos >= UART_RX_RAW_SIZE) {
            old_pos = 0;
        }
        
        UART_Start_Receiving(p_huart, rx_raw_buf, UART_RX_RAW_SIZE);
    }
}

static void UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (p_huart != NULL && huart->Instance == p_huart->Instance) {
        uart_tx_complete = true;
        UART_on_data_transmitted(huart);
    }
}

void UART_transmit(RingBuffer_t *p_uart_tx_fifo) {
    /* FIFO ➔ UART TX (DMA) */
    if (uart_tx_complete && p_uart_tx_fifo->GetCount(p_uart_tx_fifo) > 0) {
        uint16_t send_len = p_uart_tx_fifo->Read(p_uart_tx_fifo, tx_uart_active_buf, p_uart_tx_fifo->GetSize(p_uart_tx_fifo));
        if (send_len > 0) {
            uart_tx_complete = false;
            LOG_INFO("UART transmitting %d bytes", send_len);
            if (HAL_UART_Transmit_DMA(p_huart, tx_uart_active_buf, send_len) == HAL_OK) {
                LOG_INFO("UART transmitting succeeded");
            }
            else
            {
                uart_tx_complete = true; 
                LOG_ERR("UART transmitting failed!");
            }
         }
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    LOG_INFO("HAL_UARTEx_RxEventCallback");
    UART_RxEventCallback(huart, Size);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    LOG_INFO("HAL_UART_TxCpltCallback");
    UART_TxCpltCallback(huart);
}
