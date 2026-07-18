#ifndef INC_USB_UART_BRIDGE_H_
#define INC_USB_UART_BRIDGE_H_

#include "main.h"

// --- Конфігурація ---
#define BRIDGE_FIFO_SIZE       1024  // Розмір буферів (має бути ступенем двійки)
#define BRIDGE_UART_RX_RAW_SZ  256   // Розмір сирого буфера DMA для UART

// --- Публічні функції інтерфейсу ---

/**
 * @brief Ініціалізація мосту. Запускає DMA та обнуляє буфери.
 * @param huart Вказівник на структуру дескриптора UART (наприклад, &huart1)
 */
void USB_UART_Bridge_Init(UART_HandleTypeDef *huart);

/**
 * @brief Фоновий обробник мосту. Повинен викликатися в головному циклі while(1).
 */
void USB_UART_Bridge_Process(void);

/**
 * @brief Колбек для інтеграції в HAL_UARTEx_RxEventCallback.
 */
void USB_UART_Bridge_UART_RxCallback(UART_HandleTypeDef *huart, uint16_t Size);

/**
 * @brief Колбек для інтеграції в HAL_UART_TxCpltCallback.
 */
void USB_UART_Bridge_UART_TxCallback(UART_HandleTypeDef *huart);

/**
 * @brief Передача даних з USB в міст. Викликається з CDC_Receive_FS.
 */
void USB_UART_Bridge_USB_Receive(uint8_t *pbuf, uint32_t len);

#endif /* INC_USB_UART_BRIDGE_H_ */
