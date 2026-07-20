#include <stdbool.h>
#include "debug_log.h"
#include "usb_uart_bridge.h"
#include "usbd_cdc_if.h" // Потрібен для CDC_Transmit_FS та дескриптора USB
#include "usbd_def.h"

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

static volatile bool usb_rx_paused = false;
static uint8_t *p_usb_rx_buffer = NULL; // Запам'ятовуємо вказівник на USB буфер стеку

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

uint16_t FIFO_Read_Block(BridgeRingBuffer_t *buf, uint8_t *dest, uint16_t max_len) {
    uint16_t head = buf->head; // Зберігаємо в локальні змінні, бо вони volatile
    uint16_t tail = buf->tail;
    
    if (head == tail || max_len == 0) return 0;

    uint16_t available = (head - tail) & (BRIDGE_FIFO_SIZE - 1);
    uint16_t to_read = (available > max_len) ? max_len : available;
    
    // Шматок 1: від tail до кінця фізичного масиву (або до head, якщо розриву немає)
    uint16_t chunk1 = BRIDGE_FIFO_SIZE - tail;
    if (chunk1 > to_read) {
        chunk1 = to_read;
    }
    
    // Миттєве копіювання першого шматка
    memcpy(dest, &buf->data[tail], chunk1);
    
    // Шматок 2: якщо дані завернули на початок масиву
    uint16_t chunk2 = to_read - chunk1;
    if (chunk2 > 0) {
        memcpy(&dest[chunk1], &buf->data[0], chunk2);
    }
    
    // Оновлюємо покажчик tail один єдиний раз для всього блоку!
    buf->tail = (tail + to_read) & (BRIDGE_FIFO_SIZE - 1);
    
    return to_read;
}

static uint16_t FIFO_Read(BridgeRingBuffer_t *buf, uint8_t *dest, uint16_t max_len) {
    return FIFO_Read_Block(buf, dest, max_len);
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

uint8_t USB_UART_Bridge_USB_Receive(uint8_t *pbuf, uint32_t len) {
    p_usb_rx_buffer = pbuf; // Зберігаємо лінк на внутрішній буфер HAL USB

    // Перевіряємо, чи є в нашому FIFO достатньо місця для цього пакета (макс пакет = 64 байти)
    // Залишаємо запас безпеки (наприклад, 128 байт)
    uint16_t free_space = BRIDGE_FIFO_SIZE - FIFO_GetCount(&usb_to_uart_fifo);

    // Дозволяємо прийом тільки якщо є гарантоване місце для МАКСИМАЛЬНОГО пакету (64 байти)
    if (free_space > 64) {
        LOG_INFO("USB RX processing %d bytes...", len);
        // Місце є — обробляємо і записуємо
        uint16_t modified_len = USB_UART_Bridge_OnUSBReceive(pbuf, (uint16_t)len);
        if (modified_len > 0) {
            FIFO_Write(&usb_to_uart_fifo, pbuf, modified_len);
        }
        
        // Повертаємо 0 (USBD_OK), стек сам викличе ReceivePacket всередині usbd_cdc_if.c
        return USBD_OK; 
    } 
    else {
        // МІСЦЯ НЕМАЄ! Кажемо стеку, що ми зайняті.
        if (!usb_rx_paused) {
            LOG_WARN("USB RX paused, buffer full!");
            usb_rx_paused = true;
        }
        
        // Повертаємо 1 (USBD_BUSY). Стек НЕ буде викликати ReceivePacket, 
        // залізо виставить NAK, і ПК призупинить передачу!
        return USBD_BUSY; 
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
            LOG_INFO("UART transmitting %d bytes", send_len);
            if (HAL_UART_Transmit_DMA(p_huart, tx_uart_active_buf, send_len) != HAL_OK) {
                uart_tx_complete = true; 
                LOG_ERR("UART transmitting failed!");
            }
            else
                LOG_INFO("UART transmitting succeeded");
        }
    }

    /* Відновлення прийому з USB */
    if (usb_rx_paused && p_usb_rx_buffer != NULL) {
        uint16_t free_space = BRIDGE_FIFO_SIZE - FIFO_GetCount(&usb_to_uart_fifo);
        
        // Знімаємо паузу, якщо звільнилося достатньо місця (наприклад, більше половини буфера)
        if (free_space > (BRIDGE_FIFO_SIZE / 2)) {
            usb_rx_paused = false;
            LOG_WARN("USB RX restored");
             
            // Примусово перезапускаємо опитування USB точки збуту, 
            // оскільки залізо було «заморожене» через NAK статус
            USBD_CDC_SetRxBuffer(&hUsbDeviceFS, p_usb_rx_buffer);
            USBD_CDC_ReceivePacket(&hUsbDeviceFS);
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
