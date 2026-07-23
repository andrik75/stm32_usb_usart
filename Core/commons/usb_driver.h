/**
  * @file    usb_driver.c
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
  #ifndef INC_USB_DRIVER_H_
#define INC_USB_DRIVER_H_

#include <stdbool.h>
#include "usbd_def.h"
#include "ring_buffer.h"

typedef enum {
    USB_FS = 0,
    USB_HS = 1
} USBType;

typedef struct _USBD_HandleTypeDef USBD_HandleTypeDef;

typedef struct USBDriver USBDriver_t;

struct USBDriver
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
    void (*init)(USBDriver_t *self, USBType usb_type, USBD_HandleTypeDef *p_husb);
    /**
    * @brief Process USB transmission from the buffer.
    */
    void (*transmit)(USBDriver_t *self, RingBuffer_t *p_tx_fifo);
    /**
    * @brief Handle USB RX flow control unpausing.
    */
    void (*resume_rx)(USBDriver_t *self);
    void (*_receive_packet_init)(USBDriver_t *self);
    void (*on_data_transmitted)(USBDriver_t *self);
    uint16_t (*on_data_received)(USBDriver_t *self, uint8_t *p_data, uint16_t len);
};

void USBDriver_Ctor(USBDriver_t *self, USBType usb_type, USBD_HandleTypeDef *p_husb);

USBD_StatusTypeDef USB_DRIVER_CDC_FS_Receive_Callback(uint8_t *pbuf, uint32_t len, uint8_t usb_type) ;

#endif /* INC_USB_DRIVER_H_ */
