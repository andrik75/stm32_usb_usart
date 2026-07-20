#include "usb_uart_bridge.h"
#include <stddef.h>

// Shared global instances
UART_HandleTypeDef *p_huart = NULL;
BridgeRingBuffer_t usb_to_uart_fifo;
BridgeRingBuffer_t uart_to_usb_fifo;

// --- USER CODE SECTION (DATA MODIFICATION BUSINESS LOGIC) ---

/**
 * @brief Data comes from USB from PC here before being sent to UART.
 *        You can modify the 'data' array on the fly.
 * @return New data length (if changed). 0 — discard packet.
 */
__weak uint16_t USB_UART_Bridge_OnUSBReceive(uint8_t *data, uint16_t len) {
    // By default, just pass data further unchanged
    return len;
}

/**
 * @brief Data comes from UART here before being sent to USB to PC.
 */
__weak uint16_t USB_UART_Bridge_OnUARTReceive(uint8_t *data, uint16_t len) {
    // By default, just pass data further unchanged
    return len;
}

// --- Implementation of public interface ---

void USB_UART_Bridge_Init(UART_HandleTypeDef *huart) {
    BridgeRingBuffer_Ctor(&usb_to_uart_fifo);
    BridgeRingBuffer_Ctor(&uart_to_usb_fifo);

    Bridge_USB_Init();
    Bridge_UART_Init(huart);
}

void USB_UART_Bridge_Process(void) {
    if (p_huart == NULL) return;

    Bridge_UART_Process_TX();
    Bridge_USB_Process();
}
