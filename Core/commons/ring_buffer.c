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

static void FIFO_Init(RingBuffer_t *self) {
    self->head = 0;
    self->tail = 0;
}

static void FIFO_Write(RingBuffer_t *self, const uint8_t *data, uint16_t len) {
    if (len == 0 || data == NULL) 
        return;

    // 1. Fetch current volatile state into local variables
    uint16_t head;
    uint16_t tail;
    // Frame the whole function into the CRITICAL_SECTION block in case of multithreading environment
    CRITICAL_SECTION()
    {
        head = self->head; // Save to local variables because they are volatile
        tail = self->tail;
    }

    // 2. Calculate available space in the buffer
    uint16_t free_space = (tail - head - 1) & (RING_BUFFER_SIZE - 1);
    
    // Return early if no space is available
    if (free_space == 0) 
        return;

    // Clamp length to available space (overflow protection)
    uint16_t to_write = (len < free_space) ? len : free_space;

    // 3. Chunk 1: from current head to the end of the physical array
    uint16_t chunk1 = RING_BUFFER_SIZE - head;
    if (chunk1 > to_write) {
        chunk1 = to_write; // Fits entirely without wrapping around
    }

    // Fast copy for the first chunk
    memcpy(&self->data[head], data, chunk1);

    // 4. Chunk 2: wrap-around data to the beginning of the array [0]
    uint16_t chunk2 = to_write - chunk1;
    if (chunk2 > 0) {
        memcpy(&self->data[0], &data[chunk1], chunk2);
    }

    // 5. Update head pointer only once for the entire block!
    self->head = (head + to_write) & (RING_BUFFER_SIZE - 1);
}

static uint16_t FIFO_Read_Block(RingBuffer_t *self, uint8_t *dest, uint16_t max_len) {
    // Frame the whole function into the CRITICAL_SECTION block in case of multithreading environment
    uint16_t head;
    uint16_t tail;
    CRITICAL_SECTION()
    {
        head = self->head; // Save to local variables because they are volatile
        tail = self->tail;
    }
     
    if (head == tail || max_len == 0) return 0;

    uint16_t available = (head - tail) & (RING_BUFFER_SIZE - 1);
    uint16_t to_read = (available > max_len) ? max_len : available;
    
    // Chunk 1: from tail to the end of the physical array (or to head if there is no wrap-around)
    uint16_t chunk1 = RING_BUFFER_SIZE - tail;
    if (chunk1 > to_read) {
        chunk1 = to_read;
    }
    
    // Instant copy of the first chunk
    memcpy(dest, &self->data[tail], chunk1);
    
    // Chunk 2: if data wrapped around to the beginning of the array
    uint16_t chunk2 = to_read - chunk1;
    if (chunk2 > 0) {
        memcpy(&dest[chunk1], &self->data[0], chunk2);
    }
    
    // Update the tail pointer only once for the whole block!
    self->tail = (tail + to_read) & (RING_BUFFER_SIZE - 1);
    
    return to_read;
}

static uint16_t FIFO_GetCount(RingBuffer_t *self) {
    return (self->head - self->tail) & (RING_BUFFER_SIZE - 1);
}

static uint16_t FIFO_GetFreeSpace(RingBuffer_t *self) {
    return RING_BUFFER_SIZE - FIFO_GetCount(self);
}

static uint16_t FIFO_GetSize(RingBuffer_t *self) {
    return RING_BUFFER_SIZE;
}

static void FIFO_SetHead(RingBuffer_t *self, uint16_t value) {
    CRITICAL_SECTION() {
        self->head = value;
    }
}

static void FIFO_SetTail(RingBuffer_t *self, uint16_t value) {
   CRITICAL_SECTION() {
        self->tail = value;
    }
}

static void FIFO_RollbackTail(RingBuffer_t *self, uint16_t ldist) {
   CRITICAL_SECTION() {
        self->tail = (self->tail - ldist) & (RING_BUFFER_SIZE - 1);
    }
}

void RingBuffer_Ctor(RingBuffer_t *self) {
    if (self == NULL) return;

    self->Init = FIFO_Init;
    self->Write = FIFO_Write;
    self->Read = FIFO_Read_Block;
    self->GetCount = FIFO_GetCount;
    self->GetFreeSpace = FIFO_GetFreeSpace;
    self->GetSize = FIFO_GetSize;
    self->SetHead = FIFO_SetHead;
    self->SetTail = FIFO_SetTail;
    self->RollbackTail = FIFO_RollbackTail;

    self->Init(self);
}
