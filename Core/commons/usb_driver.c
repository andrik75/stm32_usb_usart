/**
  * @file    usb_driver.c
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
#include "usb_driver.h"
#include "usbd_cdc_if.h" // Needed for CDC_Transmit_FS and USB descriptor
#include "debug_log.h"

static USBDriver_t* RegisteredUSBDrivers[MAX_USBD_COUNT] = {0};

static bool Register_USBDriver(USBDriver_t* p_usb_driver) {
    for (uint8_t index = 0; index < MAX_USBD_COUNT; ++index) {
        if ((RegisteredUSBDrivers[index] == NULL) || (RegisteredUSBDrivers[index]->usb_type == p_usb_driver->usb_type)) {
            RegisteredUSBDrivers[index] = p_usb_driver;
            return true;
        }
    }
    return false;
}

static USBDriver_t* Find_USBDriver(USBType usb_type) {
    for (uint8_t index = 0; index < MAX_USBD_COUNT; ++index) {
        if (RegisteredUSBDrivers[index]->usb_type == usb_type) {
            return RegisteredUSBDrivers[index];
        }
    }
    return NULL;
}

static void USBDriver_Init(USBDriver_t *self, USBType usb_type, USBD_HandleTypeDef *p_husb) {
    self->p_owner = NULL;
    self->_rx_paused = false;
    self->_p_rx_raw_buffer = NULL;
    self->usb_type = usb_type;
    self->_p_husb = p_husb;
    RingBuffer_Ctor(&self->rx_fifo);
}

static void USBDriver_receive_packet_init(USBDriver_t *self) {
    USBD_CDC_SetRxBuffer(self->_p_husb, self->_p_rx_raw_buffer); // actually I'm not sure whether it's exactly necessary there
    USBD_CDC_ReceivePacket(self->_p_husb);
}

USBD_StatusTypeDef USB_DRIVER_CDC_FS_Receive_Callback(uint8_t *pbuf, uint32_t len, uint8_t usb_type) {
    // Log the length of the data received when debugging
    LOG_INFO("USB received %d bytes", len);
    
    USBDriver_t* p_usb_driver = Find_USBDriver(USB_FS);
    if (p_usb_driver == NULL) {
        LOG_ERR("A registered USBDriver instance has not been found for the USB type");
        return USBD_FAIL;
    }
 
    p_usb_driver->_p_rx_raw_buffer = pbuf; // Save link to internal USB HAL buffer
    uint16_t modified_len;
    if (p_usb_driver->on_data_received != NULL) {
        modified_len = p_usb_driver->on_data_received(p_usb_driver, pbuf, (uint16_t)len); // Just call the handler to process the data
    } else {
        modified_len = len;
    }
    // Check if there is enough space in our FIFO for this packet (max packet = 64 bytes)
    if (p_usb_driver->rx_fifo.GetFreeSpace(&p_usb_driver->rx_fifo) >= modified_len) {
        // Space available — process and write
        LOG_INFO("USB RX: Add %d bytes to the ring buffer", modified_len);
        if (p_usb_driver->rx_fifo.Write(&p_usb_driver->rx_fifo, pbuf, modified_len) - modified_len < 0) {
            LOG_ERR("USB RX: Some bytes were lost when writing into the ring buffer!");
        }
        p_usb_driver->_receive_packet_init(p_usb_driver);
        // Allow reception only if guaranteed space exists for the packet lenth
        // Return 0 (USBD_OK), stack itself will call ReceivePacket inside usbd_cdc_if.c
        return USBD_OK; 
    } 
    else {
        // NO SPACE! Tell stack we are busy.
        if (!p_usb_driver->_rx_paused) {
            p_usb_driver->_rx_paused = true;
            LOG_WARN("USB RX paused, buffer full!");
        }
        
        // Return 1 (USBD_BUSY). Stack will NOT call ReceivePacket, 
        // hardware will issue NAK, and PC will pause transmission!
        return USBD_BUSY; 
    }
}

static void USBDriver_Resume_RX(USBDriver_t *self) {
    /* Resume reception from USB */
    if (self->_rx_paused && self->_p_rx_raw_buffer != NULL) {
        uint16_t free_space = self->rx_fifo.GetFreeSpace(&self->rx_fifo);
        
        // Resume reception if enough space freed up (e.g., more than half the buffer)
        if (free_space >= 64) {
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

static int32_t USBDriver_transmit_data(USBDriver_t *self, uint8_t *p_data, uint16_t len) {
    int32_t result = 0;
    if (len > 0) {
        USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)self->_p_husb->pClassData;
        
        if (hcdc != NULL) {
            uint8_t transmit_result;
            while (len > 0) {
                uint16_t chunk_size = (len > 64) ? 64 : len;
                transmit_result = USBD_BUSY;
                // Attempt transmission over USB CDC
                // Check if the USB hardware is ready to accept new data
                if (hcdc->TxState == 0) {
                    switch (self->usb_type) {
                        case USB_FS:
                        transmit_result = CDC_Transmit_FS(p_data + result, chunk_size);
                        break;

                        case USB_HS:
                        transmit_result = CDC_Transmit_HS(p_data + result, chunk_size);
                        break;
                    }
                }

                if (transmit_result == USBD_OK) {
                    result += chunk_size;
                    if (self->on_data_transmitted != NULL) {
                        self->on_data_transmitted(self);
                    LOG_INFO("USB TX: %d bytes transmitted", chunk_size);
                } else {
                    // Transmission failed (busy)!
                    LOG_ERR("USB TX: Busy");
                    break;
                }
                len -= chunk_size;
            }
        }
    }
    }
    return result;
}

static int32_t USBDriver_transmit(USBDriver_t *self, RingBuffer_t *p_tx_fifo) {
    int32_t result = 0;
    uint16_t max_len = 128;

    uint16_t fifo_buf_count = p_tx_fifo->GetCount(p_tx_fifo);
    if (fifo_buf_count > 0) {
        fifo_buf_count = fifo_buf_count <= max_len ? fifo_buf_count : max_len;
        uint16_t send_len = p_tx_fifo->Read(p_tx_fifo, self->_tx_active_buf, fifo_buf_count);
        result = self->transmit_data(self, self->_tx_active_buf, send_len);
        if ((int32_t)result - (int32_t)send_len < 0) {
            p_tx_fifo->RollbackTail(p_tx_fifo, send_len - result);
         }
    }
    return result;
}

void USBDriver_Ctor(USBDriver_t *self, USBType usb_type, USBD_HandleTypeDef *p_husb) {
    if (self == NULL) return;
    if (!Register_USBDriver(self)) return;

    self->init = USBDriver_Init;
    self->_receive_packet_init = USBDriver_receive_packet_init;
    self->transmit_data = USBDriver_transmit_data;
    self->transmit = USBDriver_transmit;
    self->resume_rx = USBDriver_Resume_RX;
 
    self->init(self, usb_type, p_husb);
}
