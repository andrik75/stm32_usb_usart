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
  #ifndef INC_USB_DEVICE_H_
#define INC_USB_DEVICE_H_

#include <stdbool.h>
#include "usbd_def.h"
#include "ring_buffer.h"

typedef enum {
    USB_FS = 0,
    USB_HS = 1
} USBType;

typedef struct _USBD_HandleTypeDef USBD_HandleTypeDef;

typedef struct USBDevice USBDevice_t;

struct USBDevice
{
    // private declarations
    USBD_HandleTypeDef *_p_husb;
    volatile bool _rx_paused;
    uint8_t *_p_rx_raw_buffer; // Remember pointer to the stack's USB buffer


    // public declarations
    USBType usb_type;
    RingBuffer_t rx_fifo;
    /**
    * @brief Initialize USB interface component.
    */
    void (*init)(USBDevice_t *self, USBType usb_type, USBD_HandleTypeDef *p_husb);
    /**
    * @brief Process USB transmission from the buffer.
    */
    void (*transmit)(USBDevice_t *self, RingBuffer_t *p_tx_fifo);
    /**
    * @brief Handle USB RX flow control unpausing.
    */
    void (*resume_rx)(USBDevice_t *self);
    void (*_receive_packet_init)(USBDevice_t *self);
    void (*on_data_transmitted)(USBDevice_t *self);
    uint16_t (*on_data_received)(USBDevice_t *self, uint8_t *p_data, uint16_t len);
};

void USBDevice_Ctor(USBDevice_t *self, USBType usb_type, USBD_HandleTypeDef *p_husb);

USBD_StatusTypeDef USB_RX_TX_CDC_FS_Receive_Callback(uint8_t *pbuf, uint32_t len, uint8_t usb_type) ;

#endif /* INC_USB_DEVICE_H_ */
