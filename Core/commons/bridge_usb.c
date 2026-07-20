#include "bridge_usb.h"
#include "usb_uart_bridge.h"
#include "usbd_cdc_if.h" // Needed for CDC_Transmit_FS and USB descriptor
#include "usbd_def.h"
#include "debug_log.h"

static volatile bool usb_rx_paused = false;
static uint8_t *p_usb_rx_buffer = NULL; // Remember pointer to the stack's USB buffer

extern USBD_HandleTypeDef hUsbDeviceFS;

void Bridge_USB_Init(void) {
    usb_rx_paused = false;
    p_usb_rx_buffer = NULL;
}

uint8_t USB_UART_Bridge_USB_Receive(uint8_t *pbuf, uint32_t len) {
    p_usb_rx_buffer = pbuf; // Save link to internal USB HAL buffer

    // Check if there is enough space in our FIFO for this packet (max packet = 64 bytes)
    // Leave a safety margin (e.g. 128 bytes)
    uint16_t free_space = usb_to_uart_fifo.GetFreeSpace(&usb_to_uart_fifo);

    // Allow reception only if guaranteed space exists for MAXIMUM packet (64 bytes)
    if (free_space > 64) {
        LOG_INFO("USB RX processing %d bytes...", len);
        // Space available — process and write
        uint16_t modified_len = USB_UART_Bridge_OnUSBReceive(pbuf, (uint16_t)len);
        if (modified_len > 0) {
            usb_to_uart_fifo.Write(&usb_to_uart_fifo, pbuf, modified_len);
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

void Bridge_USB_Process(void) {
    /* Resume reception from USB */
    if (usb_rx_paused && p_usb_rx_buffer != NULL) {
        uint16_t free_space = usb_to_uart_fifo.GetFreeSpace(&usb_to_uart_fifo);
        
        // Resume reception if enough space freed up (e.g., more than half the buffer)
        if (free_space > (BRIDGE_FIFO_SIZE / 2)) {
            usb_rx_paused = false;
            LOG_WARN("USB RX restored");
             
            // Forcefully restart polling USB endpoint, 
            // as hardware was "frozen" due to NAK status
            USBD_CDC_SetRxBuffer(&hUsbDeviceFS, p_usb_rx_buffer);
            USBD_CDC_ReceivePacket(&hUsbDeviceFS);
        }
    }

    /* Path 2: FIFO ➔ USB TX (PC) */
    uint16_t usb_fifo_count = uart_to_usb_fifo.GetCount(&uart_to_usb_fifo);
    if (usb_fifo_count > 0) {
        static uint8_t temp_usb_buf[64]; // USB EndPoint packet size
        uint16_t chunk_size = (usb_fifo_count > 64) ? 64 : usb_fifo_count;
        
        USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
        if (hcdc != NULL && hcdc->TxState == 0) {
            uint16_t read_bytes = uart_to_usb_fifo.Read(&uart_to_usb_fifo, temp_usb_buf, chunk_size);
            if (read_bytes > 0) {
                CDC_Transmit_FS(temp_usb_buf, read_bytes);
            }
        }
    }
}
