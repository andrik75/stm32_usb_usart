#ifndef INC_USB_UART_BRIDGE_H_
#define INC_USB_UART_BRIDGE_H_

#include "stm32f1xx_hal.h"

// --- Public interface functions ---

/**
 * @brief Bridge initialization. Starts DMA and resets buffers.
 * @param huart Pointer to UART handle structure (e.g., &huart1)
 */
void USB_UART_Bridge_Init(UART_HandleTypeDef *huart);

/**
 * @brief Background handler of the bridge. Must be called in main loop while(1).
 */
void USB_UART_Bridge_Process(void);

#endif /* INC_USB_UART_BRIDGE_H_ */
