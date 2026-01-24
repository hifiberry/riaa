/*
 * clickdetect.c - MAD-based click detector implementation
 * 
 * Implements robust click detection using Median Absolute Deviation (MAD)
 * which adapts to signal level and is resistant to music transients.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "clickdetect.h"
#include "biquad.h"

// Comparison function for qsort
static int compare_float(const void *a, const void *b) {
    LADSPA_Data fa = *(const LADSPA_Data*)a;
    LADSPA_Data fb = *(const LADSPA_Data*)b;
    if (fa < fb) return -1;
    if (fa > fb) return 1;
    return 0;
}

LADSPA_Data clickdetect_median(LADSPA_Data *array, int size) {
    if (size == 0) return 0.0f;
    
    // Sort the array
    qsort(array, size, sizeof(LADSPA_Data), compare_float);
    
    // Return median
    if (size % 2 == 0) {
        return (array[size/2 - 1] + array[size/2]) / 2.0f;
    } else {
        return array[size/2];
    }
}

void clickdetect_config_init(ClickDetectorConfig *config, unsigned long sample_rate) {
    // Detection window: 0.5-1 ms (use 0.75ms as default)
    // This is the half-window size on each side
    config->window_size = (int)(sample_rate * 0.00075f);  // 0.75ms
    
    // MAD threshold: 6-10 typical, use 7 as default
    config->threshold = 7.0f;
    
    // Small epsilon to prevent division by zero
    config->epsilon = 1e-9f;
    
    // Max click length: 0.5-1.0 ms (use 0.75ms)
    config->max_click_length = (int)(sample_rate * 0.00075f);
    
    // Minimum energy threshold (will be adaptive in practice)
    config->min_energy = 0.0f;  // Can be tuned based on signal characteristics
    
    // High-pass filter configuration for click emphasis (always enabled)
    config->hpf_freq = 10000.0f; // 10 kHz cutoff
    config->hpf_order = 2;       // 2nd order (one biquad stage)
}

ClickDetector* clickdetect_create(const ClickDetectorConfig *config, unsigned long sample_rate) {
    ClickDetector *detector = (ClickDetector*)malloc(sizeof(ClickDetector));
    if (!detector) return NULL;
    
    // Copy configuration
    detector->config = *config;
    
    // Initialize high-pass filter (always enabled for click emphasis)
    // Determine number of stages based on order
    if (config->hpf_order == 4) {
        detector->hpf_stages = 2;  // 4th order = 2 cascaded 2nd order stages
    } else {
        detector->hpf_stages = 1;  // 2nd order or 1st order = 1 stage
    }
    
    // Generate HPF coefficients for each stage
    // For 4th order, we cascade two 2nd order Butterworth filters
    for (int i = 0; i < detector->hpf_stages; i++) {
        biquad_highpass(&detector->hpf_coeffs[i], 
                      (LADSPA_Data)sample_rate,
                      config->hpf_freq,
                      2);  // Each stage is 2nd order
    }
    
    // Initialize filter states to zero
    memset(detector->hpf_state, 0, sizeof(detector->hpf_state));
    
    // Allocate circular buffer (window on both sides + current sample)
    detector->buffer_size = config->window_size * 2 + 1;
    detector->buffer = (LADSPA_Data*)calloc(detector->buffer_size, sizeof(LADSPA_Data));
    if (!detector->buffer) {
        free(detector);
        return NULL;
    }
    
    // Allocate working arrays for median calculations
    detector->work_array = (LADSPA_Data*)malloc(detector->buffer_size * sizeof(LADSPA_Data));
    detector->deviation_array = (LADSPA_Data*)malloc(detector->buffer_size * sizeof(LADSPA_Data));
    
    if (!detector->work_array || !detector->deviation_array) {
        free(detector->buffer);
        free(detector->work_array);
        free(detector->deviation_array);
        free(detector);
        return NULL;
    }
    
    // Initialize state
    detector->write_pos = 0;
    detector->samples_filled = 0;
    detector->in_click = 0;
    detector->click_start = 0;
    detector->click_length = 0;
    detector->click_energy = 0.0f;
    
    return detector;
}

void clickdetect_free(ClickDetector *detector) {
    if (detector) {
        free(detector->buffer);
        free(detector->work_array);
        free(detector->deviation_array);
        free(detector);
    }
}

void clickdetect_reset(ClickDetector *detector) {
    if (!detector) return;
    
    memset(detector->buffer, 0, detector->buffer_size * sizeof(LADSPA_Data));
    detector->write_pos = 0;
    detector->samples_filled = 0;
    detector->in_click = 0;
    detector->click_start = 0;
    detector->click_length = 0;
    detector->click_energy = 0.0f;
    
    // Reset HPF state (always present)
    memset(detector->hpf_state, 0, sizeof(detector->hpf_state));
}

int clickdetect_process(ClickDetector *detector, LADSPA_Data sample) {
    if (!detector) return 0;
    
    // Apply high-pass filter for click emphasis
    LADSPA_Data filtered_sample = sample;
    for (int i = 0; i < detector->hpf_stages; i++) {
        filtered_sample = process_biquad(&detector->hpf_coeffs[i], 
                                        &detector->hpf_state[i], 
                                        filtered_sample);
    }
    
    // Add filtered sample to circular buffer
    detector->buffer[detector->write_pos] = filtered_sample;
    detector->write_pos = (detector->write_pos + 1) % detector->buffer_size;
    
    if (detector->samples_filled < detector->buffer_size) {
        detector->samples_filled++;
        // Need full window before we can start detecting
        return 0;
    }
    
    // Copy buffer contents to work array for median calculation
    // We need to handle the circular nature of the buffer
    int read_pos = detector->write_pos;
    for (int i = 0; i < detector->buffer_size; i++) {
        detector->work_array[i] = detector->buffer[read_pos];
        read_pos = (read_pos + 1) % detector->buffer_size;
    }
    
    // Calculate median of the window
    // Make a copy first since median calculation modifies the array
    memcpy(detector->deviation_array, detector->work_array, 
           detector->buffer_size * sizeof(LADSPA_Data));
    LADSPA_Data median = clickdetect_median(detector->deviation_array, detector->buffer_size);
    
    // Calculate absolute deviations from median
    for (int i = 0; i < detector->buffer_size; i++) {
        detector->deviation_array[i] = fabsf(detector->work_array[i] - median);
    }
    
    // Calculate MAD (Median Absolute Deviation)
    LADSPA_Data mad = clickdetect_median(detector->deviation_array, detector->buffer_size);
    
    // Get the current sample (center of window)
    LADSPA_Data current_sample = detector->work_array[detector->config.window_size];
    
    // Calculate MAD score
    LADSPA_Data score = fabsf(current_sample - median) / (mad + detector->config.epsilon);
    
    // Check if this sample exceeds threshold
    int is_candidate = (score > detector->config.threshold);
    
    // Click validation state machine
    if (is_candidate) {
        if (!detector->in_click) {
            // Start of new click candidate
            detector->in_click = 1;
            detector->click_start = 0;  // We'll use this for tracking
            detector->click_length = 1;
            detector->click_energy = fabsf(current_sample);
        } else {
            // Continue existing click
            detector->click_length++;
            detector->click_energy += fabsf(current_sample);
            
            // Check if click is too long (probably a music transient)
            if (detector->click_length > detector->config.max_click_length) {
                // Reject this click - it's too long
                detector->in_click = 0;
                detector->click_length = 0;
                detector->click_energy = 0.0f;
                return 0;
            }
        }
        return 0;  // Don't report yet, need to validate
    } else {
        // No longer a candidate
        if (detector->in_click) {
            // End of click candidate - validate it
            int is_valid_click = 1;
            
            // Check minimum energy (if configured)
            if (detector->config.min_energy > 0.0f) {
                if (detector->click_energy < detector->config.min_energy) {
                    is_valid_click = 0;
                }
            }
            
            // Reset state
            detector->in_click = 0;
            int result = is_valid_click;
            detector->click_length = 0;
            detector->click_energy = 0.0f;
            
            return result;  // Report detected click (or rejection)
        }
        return 0;  // Not in click, nothing detected
    }
}
