#ifndef INC_BRIDGE_USB_H_
#define INC_BRIDGE_USB_H_

#include <stdint.h>
#include "ring_buffer.h"
#include "usbd_def.h"

/**
 * @brief Initialize USB interface component for the bridge.
 */
void USB_RX_TX_Init(void);

/**
 * @brief Data transfer from USB into the bridge. Called from CDC_Receive_FS.
 */
USBD_StatusTypeDef __int_USB_Receive(uint8_t *pbuf, uint32_t len);

/**
 * @brief Handle USB RX flow control unpausing.
 */
void USB_Resume_RX(void);

/**
 * @brief Process USB TX transmission to PC.
 */
void USB_Process_TX(RingBuffer_t *p_usb_tx_fifo);

#endif /* INC_BRIDGE_USB_H_ */
