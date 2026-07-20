#ifndef INC_BRIDGE_RING_BUFFER_H_
#define INC_BRIDGE_RING_BUFFER_H_

#include <stdint.h>

#define RING_BUFFER_SIZE (1024) // Buffer size (must be a power of two)

typedef struct RingBuffer RingBuffer_t;

// Structure definition with OOP-style method function pointers
struct RingBuffer {
    uint8_t data[RING_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;

    void (*Init)(RingBuffer_t *self);
    void (*Write)(RingBuffer_t *self, const uint8_t *data, uint16_t len);
    uint16_t (*Read)(RingBuffer_t *self, uint8_t *dest, uint16_t max_len);
    uint16_t (*GetCount)(RingBuffer_t *self);
    uint16_t (*GetFreeSpace)(RingBuffer_t *self);
    uint16_t (*GetSize)(RingBuffer_t *self);
};

/**
 * @brief Constructor / Initialization for RingBuffer
 */
void RingBuffer_Ctor(RingBuffer_t *self);

#endif /* INC_BRIDGE_RING_BUFFER_H_ */
