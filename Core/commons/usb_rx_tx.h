/**
  * @file    usb_rx_tx.c
  * @author  Andriy Bratus <ambr75@gmail.com>
  * @brief   Header file for USB Rx/Tx implementation.
  * @date    2026
  *
  * @copyright Copyright (c) 2026 Andriy Bratus <ambr75@gmail.com>
  *            All rights reserved.
  *
  * @attention
  * SPDX-License-Identifier: GPL-3.0-or-later
  */
  #ifndef INC_USB_RX_TX_H_
#define INC_USB_RX_TX_H_

#include <stdint.h>
#include "usbd_def.h"
#include "ring_buffer.h"

/**
 * @brief Initialize USB interface component for the bridge.
 */
void USB_RX_TX_Init(void);

/**
 * @brief Data transfer from USB into the bridge. Called from CDC_Receive_FS.
 */
USBD_StatusTypeDef USB_RX_TX_CDC_Receive_Callback(uint8_t *pbuf, uint32_t len);

/**
 * @brief Handle USB RX flow control unpausing.
 */
void USB_Resume_RX(void);

/**
 * @brief Process USB TX transmission to PC.
 */
void USB_transmit(RingBuffer_t *p_usb_tx_fifo);

#endif /* INC_USB_RX_TX_H_ */
