/**
  * @file    usb_rx_tx.c
  * @author  Andriy Bratus <ambr75@gmail.com>
  * @brief   Source file for USB Rx/Tx implementation.
  * @date    2026
  *
  * @copyright Copyright (c) 2026 Andriy Bratus <ambr75@gmail.com>
  *            All rights reserved.
  *
  * @attention
  * SPDX-License-Identifier: GPL-3.0-or-later
  */
#include "config.h"
#include "usb_rx_tx.h"
#include "usbd_cdc_if.h" // Needed for CDC_Transmit_FS and USB descriptor
#include "debug_log.h"

static USBDevice_t* RegisteredUSBDevices[MAX_USBD_COUNT] = {0};

static bool Register_USBDevice(USBDevice_t* p_usb_device) {
    for (uint8_t index = 0; index < MAX_USBD_COUNT; ++index) {
        if ((RegisteredUSBDevices[index] == NULL) || (RegisteredUSBDevices[index]->usb_type == p_usb_device->usb_type)) {
            RegisteredUSBDevices[index] = p_usb_device;
            return true;
        }
    }
    return false;
}

static USBDevice_t* Find_USBDevice(USBType usb_type) {
    for (uint8_t index = 0; index < MAX_USBD_COUNT; ++index) {
        if (RegisteredUSBDevices[index]->usb_type == usb_type) {
            return RegisteredUSBDevices[index];
        }
    }
    return NULL;
}

static void USBDevice_Init(USBDevice_t *self, USBType usb_type, USBD_HandleTypeDef *p_husb) {
    self->_rx_paused = false;
    self->_p_rx_raw_buffer = NULL;
    self->usb_type = usb_type;
    self->_p_husb = p_husb;
    RingBuffer_Ctor(&self->rx_fifo);
}

static void USBDevice_receive_packet_init(USBDevice_t *self) {
    USBD_CDC_SetRxBuffer(self->_p_husb, self->_p_rx_raw_buffer); // actually I'm not sure whether it's exactly necessary there
    USBD_CDC_ReceivePacket(self->_p_husb);
}

USBD_StatusTypeDef USB_RX_TX_CDC_FS_Receive_Callback(uint8_t *pbuf, uint32_t len, uint8_t usb_type) {
    // Log the length of the data received when debugging
    LOG_INFO("USB received %d bytes", len);
    
    USBDevice_t* p_usb_device = Find_USBDevice(USB_FS);
    if (p_usb_device == NULL) {
        LOG_ERR("A registered USBDevice instance has not been found for the USB type");
        return USBD_FAIL;
    }
 
    p_usb_device->_p_rx_raw_buffer = pbuf; // Save link to internal USB HAL buffer

    // Check if there is enough space in our FIFO for this packet (max packet = 64 bytes)
    // Leave a safety margin (e.g. 128 bytes)
    uint16_t free_space = p_usb_device->rx_fifo.GetFreeSpace(&p_usb_device->rx_fifo);

    // Allow reception only if guaranteed space exists for MAXIMUM packet (64 bytes)
    if (free_space > 64) {
        LOG_INFO("USB RX: %d bytes received", len);
        // Space available — process and write
        uint16_t modified_len;
        if (p_usb_device->on_data_received != NULL) {
            modified_len = p_usb_device->on_data_received(p_usb_device, pbuf, (uint16_t)len); // Just call the handler to process the data
        } else {
            modified_len = len;
        }
        if (modified_len > 0) {
            p_usb_device->rx_fifo.Write(&p_usb_device->rx_fifo, pbuf, modified_len);
        }
        
        p_usb_device->_receive_packet_init(p_usb_device);
        // Return 0 (USBD_OK), stack itself will call ReceivePacket inside usbd_cdc_if.c
        return USBD_OK; 
    } 
    else {
        // NO SPACE! Tell stack we are busy.
        if (!p_usb_device->_rx_paused) {
            LOG_WARN("USB RX paused, buffer full!");
            p_usb_device->_rx_paused = true;
        }
        
        // Return 1 (USBD_BUSY). Stack will NOT call ReceivePacket, 
        // hardware will issue NAK, and PC will pause transmission!
        return USBD_BUSY; 
    }
}

static void USBDevice_Resume_RX(USBDevice_t *self) {
    /* Resume reception from USB */
    if (self->_rx_paused && self->_p_rx_raw_buffer != NULL) {
        uint16_t free_space = self->rx_fifo.GetFreeSpace(&self->rx_fifo);
        
        // Resume reception if enough space freed up (e.g., more than half the buffer)
        if (free_space > (self->rx_fifo.GetSize(&self->rx_fifo) / 2)) {
            self->_rx_paused = false;
            LOG_WARN("USB RX restored");
             
            // Forcefully restart polling USB endpoint, 
            // as hardware was "frozen" due to NAK status
            self->_receive_packet_init(self);
        }
    }
}

__weak uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len) {
    return USBD_FAIL;
}

__weak uint8_t CDC_Transmit_HS(uint8_t* Buf, uint16_t Len) {
    return USBD_FAIL;
}

static void USBDevice_transmit(USBDevice_t *self, RingBuffer_t *p_tx_fifo) {
    if (p_tx_fifo == NULL) return;

    uint16_t usb_fifo_count = p_tx_fifo->GetCount(p_tx_fifo);
    if (usb_fifo_count > 0) {
        USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)self->_p_husb->pClassData;
        
        // Check if the USB hardware is ready to accept new data
        if (hcdc != NULL && hcdc->TxState == 0) {
            static uint8_t temp_usb_buf[64];
            uint16_t chunk_size = (usb_fifo_count > 64) ? 64 : usb_fifo_count;
            
            // Read data from the ring buffer
            uint16_t read_bytes = p_tx_fifo->Read(p_tx_fifo, temp_usb_buf, chunk_size);
            uint8_t transmit_result;
            if (read_bytes > 0) {
                // Attempt transmission over USB CDC
                switch (self->usb_type) {
                    case USB_FS:
                    transmit_result = CDC_Transmit_FS(temp_usb_buf, read_bytes);
                    break;

                    case USB_HS:
                    transmit_result = CDC_Transmit_HS(temp_usb_buf, read_bytes);
                    break;
                }
                if (transmit_result == USBD_OK) {
                    if (self->on_data_transmitted != NULL) {
                        self->on_data_transmitted(self);
                    }
                    LOG_INFO("USB TX: %d bytes transmitted", read_bytes);
                } else {
                    // Transmission failed (busy)! Roll back the tail pointer to prevent data loss
                    p_tx_fifo->RollbackTail(p_tx_fifo, read_bytes);
                    LOG_ERR("USB TX: busy, rolling back %d bytes", read_bytes);
                }
            }
        }
    }
}

void USBDevice_Ctor(USBDevice_t *self, USBType usb_type, USBD_HandleTypeDef *p_husb) {
    if (self == NULL) return;
    if (!Register_USBDevice(self)) return;

    self->init = USBDevice_Init;
    self->_receive_packet_init = USBDevice_receive_packet_init;
    self->transmit = USBDevice_transmit;
    self->resume_rx = USBDevice_Resume_RX;
 
    self->init(self, usb_type, p_husb);
}
