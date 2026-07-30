/**
  * @file    ring_buffer.c
  * @author  Andriy Bratus <ambr75@gmail.com>
  * @brief   Source file for OOP Ring Buffer implementation.
  * @date    2026
  *
  * @copyright Copyright (c) 2026 Andriy Bratus <ambr75@gmail.com>
  *            All rights reserved.
  *
  * @attention
  * SPDX-License-Identifier: GPL-3.0-or-later
  */

#include <string.h>
#include "ring_buffer.h"
#include "critical_section.h"

static void RingBuffer_Init(RingBuffer_t *self) {
    CRITICAL_SECTION() {
        self->_head = 0;
        self->_tail = 0;
        self->_count = 0;
    }
}

static uint16_t RingBuffer_GetCount(RingBuffer_t *self) {
    uint16_t count;
    CRITICAL_SECTION() {
        count = self->_count;
    }
    return count;
}

static uint16_t RingBuffer_GetCapacity(RingBuffer_t *self) {
    return RING_BUFFER_SIZE;
}

static uint16_t RingBuffer_GetFreeSpace(RingBuffer_t *self) {
    return RING_BUFFER_SIZE - self->GetCount(self);
}

static uint16_t RingBuffer_Write(RingBuffer_t *self, const uint8_t* data, uint16_t len) {
    if (len == 0 || data == NULL)
        return 0;

    // 1. Calculate available space in the buffer
    uint16_t free_space = self->GetFreeSpace(self);

    // Return early if no space is available
    if (free_space == 0)
        return 0;

    // Clamp length to available space (overflow protection)
    uint16_t to_write = (len < free_space) ? len : free_space;

    uint16_t head;
    CRITICAL_SECTION() {
        head = self->_head;
    }

    // 2. Chunk 1: from current head to the end of the physical array
    uint16_t chunk1 = RING_BUFFER_SIZE - head;
    if (chunk1 > to_write) {
        chunk1 = to_write; // Fits entirely without wrapping around
    }

    // Fast copy for the first chunk
    memcpy(&self->_data[head], data, chunk1);

    // 3. Chunk 2: wrap-around data to the beginning of the array [0]
    uint16_t chunk2 = to_write - chunk1;
    if (chunk2 > 0) {
        memcpy(&self->_data[0], &data[chunk1], chunk2);
    }

    // 4. Update head pointer and element counter atomically
    CRITICAL_SECTION() {
        self->_head = (head + to_write) & (RING_BUFFER_SIZE - 1);
        self->_count += to_write;
    }

    return to_write;
}

static uint16_t RingBuffer_Read(RingBuffer_t *self, uint8_t *dest, uint16_t max_len) {
    if (dest == NULL || max_len == 0) return 0;

    // Frame the whole function into the CRITICAL_SECTION block in case of multithreading environment
    uint16_t count, tail;
    CRITICAL_SECTION() {
        count = self->_count;
        tail = self->_tail;
    }
     

    if (count == 0) return 0;

    uint16_t to_read = (count > max_len) ? max_len : count;

    // Chunk 1: from tail to the end of the physical array
    uint16_t chunk1 = RING_BUFFER_SIZE - tail;
    if (chunk1 > to_read) {
        chunk1 = to_read;
    }
    
    // Instant copy of the first chunk
    memcpy(dest, &self->_data[tail], chunk1);
    
    // Chunk 2: if data wrapped around to the beginning of the array
    uint16_t chunk2 = to_read - chunk1;
    if (chunk2 > 0) {
        memcpy(&dest[chunk1], &self->_data[0], chunk2);
    }
    
    // Update the tail pointer and element counter atomically
    CRITICAL_SECTION() {
        self->_tail = (tail + to_read) & (RING_BUFFER_SIZE - 1);
        self->_count -= to_read;
    }

    return to_read;
}

static void RingBuffer_SetHead(RingBuffer_t *self, uint16_t value) {
    CRITICAL_SECTION() {
        self->_head = value & (RING_BUFFER_SIZE - 1);
        self->_count = (self->_head - self->_tail) & (RING_BUFFER_SIZE - 1);
    }
}

static void RingBuffer_SetTail(RingBuffer_t *self, uint16_t value) {
    CRITICAL_SECTION() {
        self->_tail = value & (RING_BUFFER_SIZE - 1);
        self->_count = (self->_head - self->_tail) & (RING_BUFFER_SIZE - 1);
    }
}

static void RingBuffer_RollbackTail(RingBuffer_t *self, uint16_t ldist) {
    CRITICAL_SECTION() {
        self->_tail = (self->_tail - ldist) & (RING_BUFFER_SIZE - 1);
        self->_count = (self->_count + ldist) & (RING_BUFFER_SIZE - 1);
    }
}

void RingBuffer_Ctor(RingBuffer_t *self) {
    if (self == NULL) return;

    self->Init = RingBuffer_Init;
    self->Write = RingBuffer_Write;
    self->Read = RingBuffer_Read;
    self->GetCount = RingBuffer_GetCount;
    self->GetFreeSpace = RingBuffer_GetFreeSpace;
    self->GetCapacity = RingBuffer_GetCapacity;
    self->SetHead = RingBuffer_SetHead;
    self->SetTail = RingBuffer_SetTail;
    self->RollbackTail = RingBuffer_RollbackTail;

    self->Init(self);
}
