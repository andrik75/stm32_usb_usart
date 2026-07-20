#ifndef INC_BRIDGE_RING_BUFFER_H_
#define INC_BRIDGE_RING_BUFFER_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define BRIDGE_FIFO_SIZE 1024 // Buffer size (must be a power of two)

typedef struct BridgeRingBuffer BridgeRingBuffer_t;

// Structure definition with OOP-style method function pointers
struct BridgeRingBuffer {
    uint8_t data[BRIDGE_FIFO_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;

    void (*Init)(BridgeRingBuffer_t *self);
    void (*Write)(BridgeRingBuffer_t *self, const uint8_t *data, uint16_t len);
    uint16_t (*Read)(BridgeRingBuffer_t *self, uint8_t *dest, uint16_t max_len);
    uint16_t (*GetCount)(BridgeRingBuffer_t *self);
    uint16_t (*GetFreeSpace)(BridgeRingBuffer_t *self);
};

/**
 * @brief Constructor / Initialization for BridgeRingBuffer
 */
void BridgeRingBuffer_Ctor(BridgeRingBuffer_t *self);

#endif /* INC_BRIDGE_RING_BUFFER_H_ */
