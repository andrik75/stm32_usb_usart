#include "ring_buffer.h"
#include <string.h>

static void FIFO_Init(RingBuffer_t *self) {
    self->head = 0;
    self->tail = 0;
}

static void FIFO_Write(RingBuffer_t *self, const uint8_t *data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        uint16_t next_head = (self->head + 1) & (RING_BUFFER_SIZE - 1);
        if (next_head != self->tail) {
            self->data[self->head] = data[i];
            self->head = next_head;
        } else {
            break; // Overflow, ignore the rest of the packet
        }
    }
}

static uint16_t FIFO_Read_Block(RingBuffer_t *self, uint8_t *dest, uint16_t max_len) {
    uint16_t head = self->head; // Save to local variables because they are volatile
    uint16_t tail = self->tail;
    
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

void RingBuffer_Ctor(RingBuffer_t *self) {
    self->Init = FIFO_Init;
    self->Write = FIFO_Write;
    self->Read = FIFO_Read_Block;
    self->GetCount = FIFO_GetCount;
    self->GetFreeSpace = FIFO_GetFreeSpace;
    self->GetSize = FIFO_GetSize;
    
    self->Init(self);
}
