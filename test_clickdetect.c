/*
 * test_clickdetect.c - Simple test program for click detector
 * 
 * Generates test signals with synthetic clicks and verifies detection
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "clickdetect.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Generate a test signal with a click
void generate_test_signal(LADSPA_Data *buffer, int size, int sample_rate) {
    // Generate a sine wave at 1kHz
    float frequency = 1000.0f;
    float amplitude = 0.3f;
    
    for (int i = 0; i < size; i++) {
        buffer[i] = amplitude * sinf(2.0f * M_PI * frequency * i / sample_rate);
    }
    
    // Add clicks at various positions
    // Click 1: at 1/4 of buffer
    int click1_pos = size / 4;
    buffer[click1_pos] += 2.0f;      // Strong impulse
    buffer[click1_pos + 1] += 1.0f;  // Decay
    
    // Click 2: at 1/2 of buffer
    int click2_pos = size / 2;
    buffer[click2_pos] += -1.5f;
    
    // Click 3: at 3/4 of buffer (multi-sample click)
    int click3_pos = (3 * size) / 4;
    buffer[click3_pos] += 1.8f;
    buffer[click3_pos + 1] += 1.2f;
    buffer[click3_pos + 2] += 0.6f;
}

int main(void) {
    printf("Click Detector Test\n");
    printf("===================\n\n");
    
    // Test parameters
    unsigned long sample_rate = 48000;
    int buffer_size = sample_rate;  // 1 second
    
    // Allocate test buffer
    LADSPA_Data *test_buffer = (LADSPA_Data*)malloc(buffer_size * sizeof(LADSPA_Data));
    if (!test_buffer) {
        fprintf(stderr, "Failed to allocate test buffer\n");
        return 1;
    }
    
    // Generate test signal
    printf("Generating test signal (48kHz, 1 second)...\n");
    generate_test_signal(test_buffer, buffer_size, sample_rate);
    printf("  - Sine wave at 1kHz, amplitude 0.3\n");
    printf("  - Click at sample %d (strong impulse)\n", buffer_size / 4);
    printf("  - Click at sample %d (negative impulse)\n", buffer_size / 2);
    printf("  - Click at sample %d (multi-sample)\n\n", (3 * buffer_size) / 4);
    
    // Initialize click detector
    ClickDetectorConfig config;
    clickdetect_config_init(&config, sample_rate);
    
    printf("Click Detector Configuration:\n");
    printf("  - Window size: %d samples (%.2f ms)\n", 
           config.window_size, config.window_size * 1000.0f / sample_rate);
    printf("  - Threshold: %.1f\n", config.threshold);
    printf("  - Epsilon: %.2e\n", config.epsilon);
    printf("  - Max click length: %d samples (%.2f ms)\n",
           config.max_click_length, config.max_click_length * 1000.0f / sample_rate);
    printf("  - Min energy: %.2f\n", config.min_energy);
    printf("  - HPF frequency: %.1f Hz\n", config.hpf_freq);
    printf("  - HPF order: %d\n\n", config.hpf_order);
    
    ClickDetector *detector = clickdetect_create(&config, sample_rate);
    if (!detector) {
        fprintf(stderr, "Failed to create click detector\n");
        free(test_buffer);
        return 1;
    }
    
    // Process the buffer
    printf("Processing...\n\n");
    int clicks_detected = 0;
    int last_detection = -1000;  // Track to avoid duplicate reports
    
    for (int i = 0; i < buffer_size; i++) {
        int detected = clickdetect_process(detector, test_buffer[i]);
        
        if (detected && (i - last_detection) > 10) {
            clicks_detected++;
            printf("✓ Click detected at sample %d (time: %.3f ms)\n", 
                   i, i * 1000.0f / sample_rate);
            last_detection = i;
        }
    }
    
    printf("\n");
    printf("Results:\n");
    printf("  - Total clicks detected: %d\n", clicks_detected);
    printf("  - Expected: 3 clicks\n");
    
    if (clicks_detected >= 2 && clicks_detected <= 4) {
        printf("  ✓ Test PASSED (detection within expected range)\n");
    } else {
        printf("  ✗ Test FAILED (unexpected detection count)\n");
    }
    
    // Cleanup
    clickdetect_free(detector);
    free(test_buffer);
    
    return 0;
}
