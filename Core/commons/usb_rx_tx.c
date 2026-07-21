#include <stdbool.h>
#include "ring_buffer.h"
#include "usb_rx_tx.h"
#include "stm32f1xx_hal_def.h"
#include "usbd_cdc_if.h" // Needed for CDC_Transmit_FS and USB descriptor
#include "usbd_def.h"
#include "debug_log.h"

RingBuffer_t usb_rx_fifo;
static volatile bool usb_rx_paused = false;
static uint8_t *p_usb_rx_buffer = NULL; // Remember pointer to the stack's USB buffer

extern USBD_HandleTypeDef hUsbDeviceFS;

void USB_RX_TX_Init(void) {
    usb_rx_paused = false;
    p_usb_rx_buffer = NULL;
}

__weak uint16_t USB_on_receive(uint8_t *data, uint16_t len)
{
    return len;
}

USBD_StatusTypeDef __int_USB_Receive(uint8_t *pbuf, uint32_t len) {
    p_usb_rx_buffer = pbuf; // Save link to internal USB HAL buffer

    // Check if there is enough space in our FIFO for this packet (max packet = 64 bytes)
    // Leave a safety margin (e.g. 128 bytes)
    uint16_t free_space = usb_rx_fifo.GetFreeSpace(&usb_rx_fifo);

    // Allow reception only if guaranteed space exists for MAXIMUM packet (64 bytes)
    if (free_space > 64) {
        LOG_INFO("USB RX: %d bytes received", len);
        // Space available — process and write
        uint16_t modified_len = USB_on_receive(pbuf, (uint16_t)len); // Just call the handler to process the data
        if (modified_len > 0) {
            usb_rx_fifo.Write(&usb_rx_fifo, pbuf, modified_len);
        }

        // Return 0 (USBD_OK), stack itself will call ReceivePacket inside usbd_cdc_if.c
        return USBD_OK; 
    } 
    else {
        // NO SPACE! Tell stack we are busy.
        if (!usb_rx_paused) {
            LOG_WARN("USB RX paused, buffer full!");
            usb_rx_paused = true;
        }
        
        // Return 1 (USBD_BUSY). Stack will NOT call ReceivePacket, 
        // hardware will issue NAK, and PC will pause transmission!
        return USBD_BUSY; 
    }
}

void USB_Resume_RX() {
    /* Resume reception from USB */
    if (usb_rx_paused && p_usb_rx_buffer != NULL) {
        uint16_t free_space = usb_rx_fifo.GetFreeSpace(&usb_rx_fifo);
        
        // Resume reception if enough space freed up (e.g., more than half the buffer)
        if (free_space > (usb_rx_fifo.GetSize(&usb_rx_fifo) / 2)) {
            usb_rx_paused = false;
            LOG_WARN("USB RX restored");
             
            // Forcefully restart polling USB endpoint, 
            // as hardware was "frozen" due to NAK status
            USBD_CDC_ReceivePacket(&hUsbDeviceFS);
        }
    }
}

void USB_transmit(RingBuffer_t *p_usb_tx_fifo) {
    /* Path 2: FIFO ➔ USB TX (PC) */
    uint16_t usb_fifo_count = p_usb_tx_fifo->GetCount(p_usb_tx_fifo);
    if (usb_fifo_count > 0) {
        static uint8_t temp_usb_buf[64]; // USB EndPoint packet size
        uint16_t chunk_size = (usb_fifo_count > 64) ? 64 : usb_fifo_count;
        
        USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
        if (hcdc != NULL && hcdc->TxState == 0) {
            uint16_t read_bytes = p_usb_tx_fifo->Read(p_usb_tx_fifo, temp_usb_buf, chunk_size);
            if (read_bytes > 0) {
                if (CDC_Transmit_FS(temp_usb_buf, read_bytes) == USBD_OK) {
                    LOG_INFO("USB TX: %d bytes transmitted", read_bytes);
                } else {
                    LOG_ERR("USB TX: transmission failed!");
                }
            }
        }
    }
}
