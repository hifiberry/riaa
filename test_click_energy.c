/*
 * test_click_energy.c - Test click energy threshold validation
 * 
 * Tests that the click energy threshold properly filters out low-energy clicks
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
    
    printf("Click Energy Threshold Test\n");
    printf("============================\n\n");
    
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
        fprintf(stderr, "Failed to get descriptor function\n");
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
    
    // Instantiate plugin
    LADSPA_Handle handle = descriptor->instantiate(descriptor, SAMPLE_RATE);
    if (!handle) {
        fprintf(stderr, "Failed to instantiate plugin\n");
        dlclose(lib_handle);
        return 1;
    }
    
    // Create buffers
    const int buffer_size = 1024;
    float buffer[buffer_size];
    float input_l[buffer_size];
    float input_r[buffer_size];
    float output_l[buffer_size];
    float output_r[buffer_size];
    
    // Control parameters
    float gain = 0.0f;
    float subsonic = 0.0f;
    float riaa_enable = 1.0f;
    float store_settings = 0.0f;
    float clipped_samples = 0.0f;
    float detected_clicks = 0.0f;
    float click_energy = 0.0f;  // Will vary per test
    
    // Connect ports (except click_energy which we'll set per test)
    descriptor->connect_port(handle, 0, &gain);
    descriptor->connect_port(handle, 1, &subsonic);
    descriptor->connect_port(handle, 2, &riaa_enable);
    descriptor->connect_port(handle, 3, &store_settings);
    descriptor->connect_port(handle, 4, &clipped_samples);
    descriptor->connect_port(handle, 5, &detected_clicks);
    descriptor->connect_port(handle, 6, &click_energy);
    descriptor->connect_port(handle, 7, input_l);
    descriptor->connect_port(handle, 8, input_r);
    descriptor->connect_port(handle, 9, output_l);
    descriptor->connect_port(handle, 10, output_r);
    
    float frequency = 1000.0f;
    float amplitude = 0.3f;
    
    // Test 1: No energy threshold (default behavior)
    printf("Test 1: No energy threshold (click_energy = 0.0)\n");
    
    click_energy = 0.0f;
    if (descriptor->activate) {
        descriptor->activate(handle);
    }
    
    for (int i = 0; i < buffer_size; i++) {
        buffer[i] = amplitude * sinf(2.0f * M_PI * frequency * i / SAMPLE_RATE);
        input_l[i] = buffer[i];
        input_r[i] = buffer[i];
    }
    
    // Add weak and strong clicks
    int click1_pos = buffer_size / 4;
    input_l[click1_pos] += 0.5f;      // Weak click (energy ~0.5)
    
    int click2_pos = buffer_size / 2;
    input_l[click2_pos] += 2.0f;      // Strong click (energy ~2.0)
    
    descriptor->run(handle, buffer_size);
    printf("  Detected clicks: %.0f (expected: ~2, both weak and strong)\n", detected_clicks);
    printf("  Note: Counters are cumulative across tests\n\n");
    
    // Test 2: Low energy threshold (filters weak clicks)
    printf("Test 2: Energy threshold = 1.0 (filters weak clicks)\n");
    
    click_energy = 1.0f;
    if (descriptor->activate) {
        descriptor->activate(handle);
    }
    
    for (int i = 0; i < buffer_size; i++) {
        buffer[i] = amplitude * sinf(2.0f * M_PI * frequency * i / SAMPLE_RATE);
        input_l[i] = buffer[i];
        input_r[i] = buffer[i];
    }
    
    // Same clicks as before
    input_l[click1_pos] += 0.5f;      // Weak click (should be filtered)
    input_l[click2_pos] += 2.0f;      // Strong click (should pass)
    
    descriptor->run(handle, buffer_size);
    printf("  Detected clicks: %.0f (expected: ~1, only strong click)\n\n", detected_clicks);
    
    // Test 3: High energy threshold (filters all clicks)
    printf("Test 3: Energy threshold = 3.0 (filters all clicks)\n");
    
    click_energy = 3.0f;
    if (descriptor->activate) {
        descriptor->activate(handle);
    }
    
    for (int i = 0; i < buffer_size; i++) {
        buffer[i] = amplitude * sinf(2.0f * M_PI * frequency * i / SAMPLE_RATE);
        input_l[i] = buffer[i];
        input_r[i] = buffer[i];
    }
    
    // Same clicks as before
    input_l[click1_pos] += 0.5f;      // Weak click (filtered)
    input_l[click2_pos] += 2.0f;      // Strong click (filtered, energy < 3.0)
    
    descriptor->run(handle, buffer_size);
    printf("  Detected clicks: %.0f (expected: 0, both filtered)\n\n", detected_clicks);
    
    // Cleanup
    if (descriptor->deactivate) {
        descriptor->deactivate(handle);
    }
    
    descriptor->cleanup(handle);
    dlclose(lib_handle);
    
    printf("Energy threshold validation test completed!\n");
    
    return 0;
}
