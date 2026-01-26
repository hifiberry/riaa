/*
 * samplerate.h - Sample rate definitions and utilities
 */

#ifndef SAMPLERATE_H
#define SAMPLERATE_H

// Sample rate indices
#define SR_44100_IDX  0
#define SR_48000_IDX  1
#define SR_88200_IDX  2
#define SR_96000_IDX  3
#define SR_176400_IDX 4
#define SR_192000_IDX 5
#define NUM_SAMPLE_RATES 6

// Actual sample rate values
static const unsigned long SAMPLE_RATES[NUM_SAMPLE_RATES] = {
    44100, 48000, 88200, 96000, 176400, 192000
};

// Helper function to find sample rate index
static inline int get_sample_rate_index(unsigned long sample_rate) {
    for (int i = 0; i < NUM_SAMPLE_RATES; i++) {
        if (SAMPLE_RATES[i] == sample_rate) {
            return i;
        }
    }
    return -1;  // Unsupported sample rate
}

#endif // SAMPLERATE_H
