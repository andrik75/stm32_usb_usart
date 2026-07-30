/**
  * @file    ring_buffer.h
  * @author  Andriy Bratus <ambr75@gmail.com>
  * @brief   Header file for OOP Ring Buffer implementation.
  * @date    2026
  *
  * @copyright Copyright (c) 2026 Andriy Bratus <ambr75@gmail.com>
  *            All rights reserved.
  *
  * @attention
  * SPDX-License-Identifier: GPL-3.0-or-later
  */
#ifndef INC_RING_BUFFER_H_
#define INC_RING_BUFFER_H_

#include <stdint.h>

#define RING_BUFFER_SIZE (1024) // Buffer size (must be a power of two)

typedef struct RingBuffer RingBuffer_t;

// Structure definition with OOP-style method function pointers
struct RingBuffer {
    uint8_t _data[RING_BUFFER_SIZE];
    volatile uint16_t _head;
    volatile uint16_t _tail;
    volatile uint16_t _count; // Explicit count to use full RING_BUFFER_SIZE

    void (*Init)(RingBuffer_t *self);
    uint16_t (*Write)(RingBuffer_t *self, const uint8_t *data, uint16_t len);
    uint16_t (*Read)(RingBuffer_t *self, uint8_t *dest, uint16_t max_len);
    uint16_t (*GetCount)(RingBuffer_t *self);
    uint16_t (*GetFreeSpace)(RingBuffer_t *self);
    uint16_t (*GetCapacity)(RingBuffer_t *self);
    void (*SetHead)(RingBuffer_t *self, uint16_t value);
    void (*SetTail)(RingBuffer_t *self, uint16_t value);
    void (*RollbackTail)(RingBuffer_t *self, uint16_t ldist); // Positive value of the ldist paramter means ldist bytes back
};

/**
 * @brief Constructor / Initialization for RingBuffer
 */
void RingBuffer_Ctor(RingBuffer_t *self);

#endif /* INC_RING_BUFFER_H_ */
