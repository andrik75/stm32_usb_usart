#ifndef INC_USB_UART_BRIDGE_H_
#define INC_USB_UART_BRIDGE_H_

#include "main.h"
#include "bridge_ring_buffer.h"
#include "bridge_uart.h"
#include "bridge_usb.h"

// Shared data objects used across bridge modules
extern UART_HandleTypeDef *p_huart;
extern BridgeRingBuffer_t usb_to_uart_fifo;
extern BridgeRingBuffer_t uart_to_usb_fifo;

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

// --- USER CODE SECTION (BUSINESS LOGIC FOR DATA MODIFICATION) ---

/**
 * @brief Data received from USB from PC before sending to UART.
 *        You can modify the 'data' array on the fly.
 * @return New data length (if changed). 0 — discard packet.
 */
uint16_t USB_UART_Bridge_OnUSBReceive(uint8_t *data, uint16_t len);

/**
 * @brief Data received from UART before sending to USB to PC.
 */
uint16_t USB_UART_Bridge_OnUARTReceive(uint8_t *data, uint16_t len);

#endif /* INC_USB_UART_BRIDGE_H_ */
