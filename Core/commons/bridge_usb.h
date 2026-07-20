#ifndef INC_BRIDGE_USB_H_
#define INC_BRIDGE_USB_H_

#include "main.h"

/**
 * @brief Initialize USB interface component for the bridge.
 */
void Bridge_USB_Init(void);

/**
 * @brief Data transfer from USB into the bridge. Called from CDC_Receive_FS.
 */
uint8_t USB_UART_Bridge_USB_Receive(uint8_t *pbuf, uint32_t len);

/**
 * @brief Process USB TX transmission to PC and handle USB RX flow control unpausing.
 */
void Bridge_USB_Process(void);

#endif /* INC_BRIDGE_USB_H_ */
