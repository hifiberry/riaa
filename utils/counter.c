/*
 * counter.c - 64-bit counter implementation
 * 
 * Simple 64-bit counter for tracking events (e.g., clipped samples)
 */

#include "counter.h"

// Initialize a counter to 0
void counter_init(Counter *counter) {
    if (counter) {
        counter->count = 0;
    }
}

// Increment the counter by 1
void counter_increment(Counter *counter) {
    if (counter) {
        counter->count++;
    }
}

// Get the current count value
uint64_t counter_get(const Counter *counter) {
    return counter ? counter->count : 0;
}

// Reset the counter to 0
void counter_reset(Counter *counter) {
    if (counter) {
        counter->count = 0;
    }
}
