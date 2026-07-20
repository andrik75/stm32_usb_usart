#ifndef INC_BRIDGE_UART_H_
#define INC_BRIDGE_UART_H_

#include "main.h"

#define BRIDGE_UART_RX_RAW_SZ  256   // Size of the raw DMA buffer for UART

/**
 * @brief Initialize UART interface component for the bridge.
 */
void Bridge_UART_Init(UART_HandleTypeDef *huart);

/**
 * @brief Callback for integration into HAL_UARTEx_RxEventCallback.
 */
void USB_UART_Bridge_UART_RxCallback(UART_HandleTypeDef *huart, uint16_t Size);

/**
 * @brief Callback for integration into HAL_UART_TxCpltCallback.
 */
void USB_UART_Bridge_UART_TxCallback(UART_HandleTypeDef *huart);

/**
 * @brief Process UART transmission from FIFO.
 */
void Bridge_UART_Process_TX(void);

#endif /* INC_BRIDGE_UART_H_ */
