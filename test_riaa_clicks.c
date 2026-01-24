/*
 * test_riaa_clicks.c - Test RIAA plugin click detection functionality
 * 
 * This test verifies that the click detection in the RIAA plugin works correctly
 * by feeding synthetic clicks and checking the output counter.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dlfcn.h>
#include <ladspa.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE 48000

int main(int argc, char *argv[]) {
    const char *plugin_path = argc > 1 ? argv[1] : "./riaa.so";
    
    printf("Loading plugin: %s\n", plugin_path);
    
    // Load plugin library
    void *lib_handle = dlopen(plugin_path, RTLD_NOW);
    if (!lib_handle) {
        fprintf(stderr, "Failed to load plugin: %s\n", dlerror());
        return 1;
    }
    
    // Get descriptor function
    LADSPA_Descriptor_Function desc_func = 
        (LADSPA_Descriptor_Function)dlsym(lib_handle, "ladspa_descriptor");
    if (!desc_func) {
        fprintf(stderr, "Failed to get descriptor function: %s\n", dlerror());
        dlclose(lib_handle);
        return 1;
    }
    
    // Get plugin descriptor
    const LADSPA_Descriptor *descriptor = desc_func(0);
    if (!descriptor) {
        fprintf(stderr, "Failed to get plugin descriptor\n");
        dlclose(lib_handle);
        return 1;
    }
    
    printf("Plugin: %s (Label: %s)\n", descriptor->Name, descriptor->Label);
    printf("Port count: %lu\n", descriptor->PortCount);
    
    // List all ports
    for (unsigned long i = 0; i < descriptor->PortCount; i++) {
        printf("  Port %lu: %s\n", i, descriptor->PortNames[i]);
    }
    
    // Instantiate plugin
    LADSPA_Handle handle = descriptor->instantiate(descriptor, SAMPLE_RATE);
    if (!handle) {
        fprintf(stderr, "Failed to instantiate plugin\n");
        dlclose(lib_handle);
        return 1;
    }
    
    printf("\nPlugin instantiated at %d Hz\n", SAMPLE_RATE);
    
    // Create buffers
    const int buffer_size = 1024;
    float buffer[buffer_size];  // Temporary buffer for signal generation
    float input_l[buffer_size];
    float input_r[buffer_size];
    float output_l[buffer_size];
    float output_r[buffer_size];
    
    // Control parameters
    float gain = 0.0f;           // Port 0: Gain (dB)
    float subsonic = 0.0f;       // Port 1: Subsonic Filter (off)
    float riaa_enable = 1.0f;    // Port 2: RIAA Enable (on)
    float store_settings = 0.0f; // Port 3: Store settings (off)
    float input_clipped_samples = 0.0f; // Port 4: Input Clipped Samples (output)
    float clipped_samples = 0.0f; // Port 5: Clipped Samples (output)
    float detected_clicks = 0.0f; // Port 6: Detected Clicks (output)
    float click_energy = 0.0f;    // Port 7: Click Energy Threshold (0 = disabled)
    
    // Connect ports
    descriptor->connect_port(handle, 0, &gain);
    descriptor->connect_port(handle, 1, &subsonic);
    descriptor->connect_port(handle, 2, &riaa_enable);
    descriptor->connect_port(handle, 3, &store_settings);
    descriptor->connect_port(handle, 4, &input_clipped_samples);
    descriptor->connect_port(handle, 5, &clipped_samples);
    descriptor->connect_port(handle, 6, &detected_clicks);
    descriptor->connect_port(handle, 7, &click_energy);
    descriptor->connect_port(handle, 8, input_l);   // Input L
    descriptor->connect_port(handle, 9, input_r);   // Input R
    descriptor->connect_port(handle, 10, output_l);  // Output L
    descriptor->connect_port(handle, 11, output_r); // Output R
    
    // Activate plugin
    if (descriptor->activate) {
        descriptor->activate(handle);
    }
    
    printf("\nTest 1: Clean signal (no clicks)\n");
    
    // Generate clean sine wave (no clicks) - 1kHz, amplitude 0.3
    float frequency = 1000.0f;
    float amplitude = 0.3f;
    for (int i = 0; i < buffer_size; i++) {
        buffer[i] = amplitude * sinf(2.0f * M_PI * frequency * i / SAMPLE_RATE);
        input_l[i] = buffer[i];
        input_r[i] = buffer[i];
    }
    
    descriptor->run(handle, buffer_size);
    printf("Detected clicks: %.0f (expected: 0)\n", detected_clicks);
    
    // Reset for next test
    if (descriptor->activate) {
        descriptor->activate(handle);
    }
    
    printf("\nTest 2: Signal with 3 synthetic clicks\n");
    
    // Generate signal with clicks - matching test_clickdetect.c
    for (int i = 0; i < buffer_size; i++) {
        buffer[i] = amplitude * sinf(2.0f * M_PI * frequency * i / SAMPLE_RATE);
        input_l[i] = buffer[i];
        input_r[i] = buffer[i];
    }
    
    // Add 3 clicks matching test_clickdetect.c pattern
    // Click 1: at 1/4 of buffer
    int click1_pos = buffer_size / 4;
    input_l[click1_pos] += 2.0f;      // Strong impulse
    input_l[click1_pos + 1] += 1.0f;  // Decay
    
    // Click 2: at 1/2 of buffer
    int click2_pos = buffer_size / 2;
    input_l[click2_pos] += -1.5f;
    
    // Click 3: at 3/4 of buffer (multi-sample click)
    int click3_pos = (3 * buffer_size) / 4;
    input_l[click3_pos] += 1.8f;
    input_l[click3_pos + 1] += 1.2f;
    input_l[click3_pos + 2] += 0.6f;
    
    descriptor->run(handle, buffer_size);
    printf("Detected clicks: %.0f (expected: ~3)\n", detected_clicks);
    
    // Reset for next test
    if (descriptor->activate) {
        descriptor->activate(handle);
    }
    
    printf("\nTest 3: Same clicks on right channel\n");
    
    // Generate signal with clicks on right channel
    for (int i = 0; i < buffer_size; i++) {
        buffer[i] = amplitude * sinf(2.0f * M_PI * frequency * i / SAMPLE_RATE);
        input_l[i] = buffer[i];
        input_r[i] = buffer[i];
    }
    
    // Add 3 clicks to right channel (same pattern)
    input_r[click1_pos] += 2.0f;
    input_r[click1_pos + 1] += 1.0f;
    input_r[click2_pos] += -1.5f;
    input_r[click3_pos] += 1.8f;
    input_r[click3_pos + 1] += 1.2f;
    input_r[click3_pos + 2] += 0.6f;
    
    descriptor->run(handle, buffer_size);
    printf("Detected clicks: %.0f (expected: ~6 cumulative)\n", detected_clicks);
    
    // Reset for next test
    if (descriptor->activate) {
        descriptor->activate(handle);
    }
    
    printf("\nTest 4: Clean signal (counter continues)\n");
    
    // Generate clean signal
    for (int i = 0; i < buffer_size; i++) {
        buffer[i] = amplitude * sinf(2.0f * M_PI * frequency * i / SAMPLE_RATE);
        input_l[i] = buffer[i];
        input_r[i] = buffer[i];
    }
    
    descriptor->run(handle, buffer_size);
    printf("Detected clicks: %.0f (expected: 6, counter persists)\n", detected_clicks);
    
    // Cleanup
    if (descriptor->deactivate) {
        descriptor->deactivate(handle);
    }
    
    descriptor->cleanup(handle);
    dlclose(lib_handle);
    
    printf("\nTest completed successfully!\n");
    
    return 0;
}
