#include <stdbool.h>
#include "usb_uart_bridge.h"
#include "usbd_cdc_if.h" // Потрібен для CDC_Transmit_FS та дескриптора USB

// Внутрішня структура FIFO
typedef struct {
    uint8_t data[BRIDGE_FIFO_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} BridgeRingBuffer_t;

// --- Приватні змінні модуля ---
static UART_HandleTypeDef *p_huart = NULL;
static BridgeRingBuffer_t usb_to_uart_fifo = {0};
static BridgeRingBuffer_t uart_to_usb_fifo = {0};

static uint8_t rx_raw_buf[BRIDGE_UART_RX_RAW_SZ];
static uint32_t old_pos = 0;

static uint8_t tx_uart_active_buf[BRIDGE_FIFO_SIZE];
static volatile bool uart_tx_complete = true;

extern USBD_HandleTypeDef hUsbDeviceFS;

// --- Приватні функції кільцевого буфера ---
static void FIFO_Write(BridgeRingBuffer_t *buf, const uint8_t *data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        uint16_t next_head = (buf->head + 1) & (BRIDGE_FIFO_SIZE - 1);
        if (next_head != buf->tail) {
            buf->data[buf->head] = data[i];
            buf->head = next_head;
        } else {
            break; // Переповнення, ігноруємо залишок пакета
        }
    }
}

static uint16_t FIFO_Read(BridgeRingBuffer_t *buf, uint8_t *data, uint16_t max_len) {
    uint16_t count = 0;
    while (buf->head != buf->tail && count < max_len) {
        data[count++] = buf->data[buf->tail];
        buf->tail = (buf->tail + 1) & (BRIDGE_FIFO_SIZE - 1);
    }
    return count;
}

static uint16_t FIFO_GetCount(BridgeRingBuffer_t *buf) {
    return (buf->head - buf->tail) & (BRIDGE_FIFO_SIZE - 1);
}

// --- СЕКЦІЯ ЮЗЕР-КОД (БІЗНЕС ЛОГІКА МОДИФІКАЦІЇ ДАНИХ) ---

/**
 * @brief Сюди приходять дані з USB від ПК перед відправкою в UART.
 *        Ви можете змінювати масив 'data' на льоту.
 * @return Нова довжина даних (якщо вона змінилася). 0 — видалити пакет.
 */
__weak uint16_t USB_UART_Bridge_OnUSBReceive(uint8_t *data, uint16_t len) {
    // За замовчуванням просто пропускаємо дані далі без змін
    return len;
}

/**
 * @brief Сюди приходять дані з UART перед відправкою в USB на ПК.
 */
__weak uint16_t USB_UART_Bridge_OnUARTReceive(uint8_t *data, uint16_t len) {
    // За замовчуванням просто пропускаємо дані далі без змін
    return len;
}

// --- Реалізація публічного інтерфейсу ---

void USB_UART_Bridge_Init(UART_HandleTypeDef *huart) {
    p_huart = huart;
    usb_to_uart_fifo.head = 0;
    usb_to_uart_fifo.tail = 0;
    uart_to_usb_fifo.head = 0;
    uart_to_usb_fifo.tail = 0;
    old_pos = 0;
    uart_tx_complete = true;

    // Перший запуск кругового приймання через DMA з Idle детекцією
    HAL_UARTEx_ReceiveToIdle_DMA(p_huart, rx_raw_buf, BRIDGE_UART_RX_RAW_SZ);
    __HAL_DMA_DISABLE_IT(p_huart->hdmarx, DMA_IT_HT); 
}

void USB_UART_Bridge_USB_Receive(uint8_t *pbuf, uint32_t len) {
    // Обробка через бізнес-логіку користувача
    uint16_t modified_len = USB_UART_Bridge_OnUSBReceive(pbuf, (uint16_t)len);
    if (modified_len > 0) {
        FIFO_Write(&usb_to_uart_fifo, pbuf, modified_len);
    }
}

void USB_UART_Bridge_UART_RxCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    if (p_huart != NULL && huart->Instance == p_huart->Instance) {
        uint16_t write_pos = Size;
        if (write_pos != old_pos) {
            uint16_t len = 0;
            uint8_t temp_proc_buf[BRIDGE_UART_RX_RAW_SZ];

            if (write_pos > old_pos) {
                len = write_pos - old_pos;
                memcpy(temp_proc_buf, &rx_raw_buf[old_pos], len);
            } else {
                len = BRIDGE_UART_RX_RAW_SZ - old_pos;
                memcpy(temp_proc_buf, &rx_raw_buf[old_pos], len);
                if (write_pos > 0) {
                    memcpy(&temp_proc_buf[len], &rx_raw_buf[0], write_pos);
                    len += write_pos;
                }
            }

            // Модифікація бізнес-логікою перед записом у FIFO
            uint16_t modified_len = USB_UART_Bridge_OnUARTReceive(temp_proc_buf, len);
            if (modified_len > 0) {
                FIFO_Write(&uart_to_usb_fifo, temp_proc_buf, modified_len);
            }
            old_pos = write_pos;
        }

        if (old_pos >= BRIDGE_UART_RX_RAW_SZ) {
            old_pos = 0;
        }
        
        HAL_UARTEx_ReceiveToIdle_DMA(p_huart, rx_raw_buf, BRIDGE_UART_RX_RAW_SZ);
        __HAL_DMA_DISABLE_IT(p_huart->hdmarx, DMA_IT_HT);
    }
}

void USB_UART_Bridge_UART_TxCallback(UART_HandleTypeDef *huart) {
    if (p_huart != NULL && huart->Instance == p_huart->Instance) {
        uart_tx_complete = true;
    }
}

void USB_UART_Bridge_Process(void) {
    if (p_huart == NULL) return;

    /* Шлях 1: FIFO ➔ UART TX (DMA) */
    if (uart_tx_complete && FIFO_GetCount(&usb_to_uart_fifo) > 0) {
        uint16_t send_len = FIFO_Read(&usb_to_uart_fifo, tx_uart_active_buf, BRIDGE_FIFO_SIZE);
        if (send_len > 0) {
            uart_tx_complete = false;
            if (HAL_UART_Transmit_DMA(p_huart, tx_uart_active_buf, send_len) != HAL_OK) {
                uart_tx_complete = true; 
            }
        }
    }

    /* Шлях 2: FIFO ➔ USB TX (ПК) */
    uint16_t usb_fifo_count = FIFO_GetCount(&uart_to_usb_fifo);
    if (usb_fifo_count > 0) {
        static uint8_t temp_usb_buf[64]; // Розмір пакета USB EndPoint
        uint16_t chunk_size = (usb_fifo_count > 64) ? 64 : usb_fifo_count;
        
        USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
        if (hcdc != NULL && hcdc->TxState == 0) {
            uint16_t read_bytes = FIFO_Read(&uart_to_usb_fifo, temp_usb_buf, chunk_size);
            if (read_bytes > 0) {
                CDC_Transmit_FS(temp_usb_buf, read_bytes);
            }
        }
    }
}
