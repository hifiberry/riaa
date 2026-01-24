/*
 * clickdetect.h - MAD-based click detector for vinyl de-clicking
 * 
 * Uses Median Absolute Deviation (MAD) for robust, adaptive click detection
 * that automatically adjusts to signal level without fixed thresholds.
 */

#ifndef CLICKDETECT_H
#define CLICKDETECT_H

#include <ladspa.h>
#include "biquad.h"

// Click detector configuration
typedef struct {
    int window_size;           // Detection window size in samples (half-window on each side)
    LADSPA_Data threshold;     // MAD threshold multiplier (6-10 typical)
    LADSPA_Data epsilon;       // Small value to prevent division by zero
    int max_click_length;      // Maximum click length in samples (0.5-1ms)
    LADSPA_Data min_energy;    // Minimum energy threshold for valid clicks
    LADSPA_Data hpf_freq;      // HPF cutoff frequency (10-12 kHz typical)
    int hpf_order;             // HPF order (2 or 4)
} ClickDetectorConfig;

// Click detector state
typedef struct {
    ClickDetectorConfig config;
    
    // High-pass filter for click emphasis
    BiquadCoeffs hpf_coeffs[2];  // Up to 2 cascaded biquads for 4th order
    BiquadState hpf_state[2];
    int hpf_stages;                // Number of biquad stages (1 or 2)
    
    // Circular buffer for windowed processing
    LADSPA_Data *buffer;
    int buffer_size;
    int write_pos;
    int samples_filled;
    
    // Working arrays for median calculation
    LADSPA_Data *work_array;
    LADSPA_Data *deviation_array;
    
    // Click candidate tracking
    int in_click;              // Are we currently in a click?
    int click_start;           // Start position of current click candidate
    int click_length;          // Length of current click candidate
    LADSPA_Data click_energy;  // Accumulated energy of current click
} ClickDetector;

/**
 * Initialize click detector configuration with sensible defaults
 * 
 * @param config Configuration structure to initialize
 * @param sample_rate Sample rate in Hz
 */
void clickdetect_config_init(ClickDetectorConfig *config, unsigned long sample_rate);

/**
 * Create a new click detector instance
 * 
 * @param config Configuration parameters
 * @param sample_rate Sample rate in Hz (needed for HPF coefficient generation)
 * @return Pointer to new detector, or NULL on allocation failure
 */
ClickDetector* clickdetect_create(const ClickDetectorConfig *config, unsigned long sample_rate);

/**
 * Free a click detector instance
 * 
 * @param detector Detector to free
 */
void clickdetect_free(ClickDetector *detector);

/**
 * Reset click detector state (clear buffers, reset tracking)
 * 
 * @param detector Detector to reset
 */
void clickdetect_reset(ClickDetector *detector);

/**
 * Process a single sample through the click detector
 * 
 * This uses MAD (Median Absolute Deviation) for robust, adaptive detection:
 * 1. Calculate median of samples in window around current sample
 * 2. Calculate MAD (median of absolute deviations from median)
 * 3. Score = |sample - median| / (MAD + epsilon)
 * 4. If score > threshold, mark as click candidate
 * 5. Validate click length and energy before confirming
 * 
 * @param detector Detector instance
 * @param sample Input sample (typically high-pass filtered)
 * @return 1 if click detected at this sample, 0 otherwise
 */
int clickdetect_process(ClickDetector *detector, LADSPA_Data sample);

/**
 * Helper function: Calculate median of an array
 * Note: This modifies the input array (sorts it)
 * 
 * @param array Array of values
 * @param size Number of elements
 * @return Median value
 */
LADSPA_Data clickdetect_median(LADSPA_Data *array, int size);

#endif // CLICKDETECT_H
