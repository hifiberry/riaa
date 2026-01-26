/*
 * counter.h - 64-bit counter utilities
 * 
 * Simple 64-bit counter for tracking events (e.g., clipped samples)
 */

#ifndef COUNTER_H
#define COUNTER_H

#include <stdint.h>

typedef struct {
    uint64_t count;
} Counter;

// Initialize a counter to 0
void counter_init(Counter *counter);

// Increment the counter by 1
void counter_increment(Counter *counter);

// Get the current count value
uint64_t counter_get(const Counter *counter);

// Reset the counter to 0
void counter_reset(Counter *counter);

#endif // COUNTER_H
