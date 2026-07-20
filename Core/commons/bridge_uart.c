#include "bridge_uart.h"
#include "usb_uart_bridge.h"
#include "debug_log.h"
#include <string.h>

static uint8_t rx_raw_buf[BRIDGE_UART_RX_RAW_SZ];
static uint32_t old_pos = 0;

static uint8_t tx_uart_active_buf[BRIDGE_FIFO_SIZE];
static volatile bool uart_tx_complete = true;

void Bridge_UART_Init(UART_HandleTypeDef *huart) {
    p_huart = huart;
    old_pos = 0;
    uart_tx_complete = true;

    // Initial launch of circular reception via DMA with Idle detection
    HAL_UARTEx_ReceiveToIdle_DMA(p_huart, rx_raw_buf, BRIDGE_UART_RX_RAW_SZ);
    __HAL_DMA_DISABLE_IT(p_huart->hdmarx, DMA_IT_HT); 
}

void USB_UART_Bridge_UART_RxCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    if (p_huart != NULL && huart->Instance == p_huart->Instance) {
        uint16_t write_pos = Size;
        if (write_pos != old_pos) {
            uint16_t len = 0;
            uint8_t temp_proc_buf[BRIDGE_UART_RX_RAW_SZ];

            if (write_pos > old_pos) {
                len = write_pos - old_pos;
                memcpy(temp_proc_buf, &rx_raw_buf[old_pos], len);
            } else {
                len = BRIDGE_UART_RX_RAW_SZ - old_pos;
                memcpy(temp_proc_buf, &rx_raw_buf[old_pos], len);
                if (write_pos > 0) {
                    memcpy(&temp_proc_buf[len], &rx_raw_buf[0], write_pos);
                    len += write_pos;
                }
            }

            // Modification by business logic before writing to FIFO
            uint16_t modified_len = USB_UART_Bridge_OnUARTReceive(temp_proc_buf, len);
            if (modified_len > 0) {
                uart_to_usb_fifo.Write(&uart_to_usb_fifo, temp_proc_buf, modified_len);
            }
            old_pos = write_pos;
        }

        if (old_pos >= BRIDGE_UART_RX_RAW_SZ) {
            old_pos = 0;
        }
        
        HAL_UARTEx_ReceiveToIdle_DMA(p_huart, rx_raw_buf, BRIDGE_UART_RX_RAW_SZ);
        __HAL_DMA_DISABLE_IT(p_huart->hdmarx, DMA_IT_HT);
    }
}

void USB_UART_Bridge_UART_TxCallback(UART_HandleTypeDef *huart) {
    if (p_huart != NULL && huart->Instance == p_huart->Instance) {
        uart_tx_complete = true;
    }
}

void Bridge_UART_Process_TX(void) {
    /* Path 1: FIFO ➔ UART TX (DMA) */
    if (uart_tx_complete && usb_to_uart_fifo.GetCount(&usb_to_uart_fifo) > 0) {
        uint16_t send_len = usb_to_uart_fifo.Read(&usb_to_uart_fifo, tx_uart_active_buf, BRIDGE_FIFO_SIZE);
        if (send_len > 0) {
            uart_tx_complete = false;
            LOG_INFO("UART transmitting %d bytes", send_len);
            if (HAL_UART_Transmit_DMA(p_huart, tx_uart_active_buf, send_len) != HAL_OK) {
                uart_tx_complete = true; 
                LOG_ERR("UART transmitting failed!");
            }
            else
                LOG_INFO("UART transmitting succeeded");
        }
    }
}
