/*
 * riaa_process.c - Process audio file with RIAA plugin and show statistics
 * 
 * Usage: riaa_process input.wav output.wav [gain] [subsonic] [riaa_enable] [declick_enable] [spike_threshold] [spike_width]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sndfile.h>
#include <ladspa.h>

#define BUFFER_SIZE 8192

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s input.wav output.wav [gain] [subsonic] [riaa_enable] [declick_enable] [spike_threshold] [spike_width]\n", argv[0]);
        fprintf(stderr, "\nDefaults:\n");
        fprintf(stderr, "  gain: 0.0 dB\n");
        fprintf(stderr, "  subsonic: 0 (off)\n");
        fprintf(stderr, "  riaa_enable: 1 (on)\n");
        fprintf(stderr, "  declick_enable: 0 (off)\n");
        fprintf(stderr, "  spike_threshold: 15.0 dB\n");
        fprintf(stderr, "  spike_width: 1.0 ms\n");
        return 1;
    }
    
    // Parse command line arguments with defaults
    float gain = (argc > 3) ? atof(argv[3]) : 0.0f;
    float subsonic = (argc > 4) ? atof(argv[4]) : 0.0f;
    float riaa_enable = (argc > 5) ? atof(argv[5]) : 1.0f;
    float declick_enable = (argc > 6) ? atof(argv[6]) : 0.0f;
    float spike_threshold = (argc > 7) ? atof(argv[7]) : 150.0f;
    float spike_width = (argc > 8) ? atof(argv[8]) : 1.0f;
    float store_settings = 0.0f;
    
    // Output control ports
    float clipped_samples = 0.0f;
    float detected_clicks = 0.0f;
    float avg_spike_length = 0.0f;
    float avg_rms_db = 0.0f;
    
    // Open input file
    SF_INFO sf_info_in;
    memset(&sf_info_in, 0, sizeof(SF_INFO));
    SNDFILE *sf_in = sf_open(argv[1], SFM_READ, &sf_info_in);
    if (!sf_in) {
        fprintf(stderr, "Error: Could not open input file '%s'\n", argv[1]);
        fprintf(stderr, "%s\n", sf_strerror(NULL));
        return 1;
    }
    
    if (sf_info_in.channels != 2) {
        fprintf(stderr, "Error: Input file must be stereo (2 channels)\n");
        sf_close(sf_in);
        return 1;
    }
    
    printf("Input: %s\n", argv[1]);
    printf("  Sample rate: %d Hz\n", sf_info_in.samplerate);
    printf("  Channels: %d\n", sf_info_in.channels);
    printf("  Frames: %ld\n", (long)sf_info_in.frames);
    printf("\n");
    
    // Open output file
    SF_INFO sf_info_out = sf_info_in;
    SNDFILE *sf_out = sf_open(argv[2], SFM_WRITE, &sf_info_out);
    if (!sf_out) {
        fprintf(stderr, "Error: Could not open output file '%s'\n", argv[2]);
        fprintf(stderr, "%s\n", sf_strerror(NULL));
        sf_close(sf_in);
        return 1;
    }
    
    // Load LADSPA plugin
    void *plugin_lib = dlopen("/usr/local/lib/ladspa/riaa.so", RTLD_NOW);
    if (!plugin_lib) {
        fprintf(stderr, "Error: Could not load LADSPA plugin\n");
        fprintf(stderr, "%s\n", dlerror());
        sf_close(sf_in);
        sf_close(sf_out);
        return 1;
    }
    
    LADSPA_Descriptor_Function descriptor_fn = 
        (LADSPA_Descriptor_Function)dlsym(plugin_lib, "ladspa_descriptor");
    if (!descriptor_fn) {
        fprintf(stderr, "Error: Could not find ladspa_descriptor\n");
        dlclose(plugin_lib);
        sf_close(sf_in);
        sf_close(sf_out);
        return 1;
    }
    
    const LADSPA_Descriptor *descriptor = descriptor_fn(0);
    if (!descriptor) {
        fprintf(stderr, "Error: Could not get plugin descriptor\n");
        dlclose(plugin_lib);
        sf_close(sf_in);
        sf_close(sf_out);
        return 1;
    }
    
    printf("Plugin: %s\n", descriptor->Name);
    printf("Label: %s\n", descriptor->Label);
    printf("\n");
    
    // Instantiate plugin
    LADSPA_Handle handle = descriptor->instantiate(descriptor, sf_info_in.samplerate);
    if (!handle) {
        fprintf(stderr, "Error: Could not instantiate plugin\n");
        dlclose(plugin_lib);
        sf_close(sf_in);
        sf_close(sf_out);
        return 1;
    }
    
    // Allocate buffers
    float *input_l = (float *)malloc(BUFFER_SIZE * sizeof(float));
    float *input_r = (float *)malloc(BUFFER_SIZE * sizeof(float));
    float *output_l = (float *)malloc(BUFFER_SIZE * sizeof(float));
    float *output_r = (float *)malloc(BUFFER_SIZE * sizeof(float));
    float *interleaved = (float *)malloc(BUFFER_SIZE * 2 * sizeof(float));
    
    // Connect ports (based on port order in riaa_ladspa.c)
    descriptor->connect_port(handle, 0, &gain);                 // Gain
    descriptor->connect_port(handle, 1, &subsonic);             // Subsonic Filter
    descriptor->connect_port(handle, 2, &riaa_enable);          // RIAA Enable
    descriptor->connect_port(handle, 3, &declick_enable);       // Declick Enable
    descriptor->connect_port(handle, 4, &spike_threshold);      // Spike Threshold
    descriptor->connect_port(handle, 5, &spike_width);          // Spike Width
    descriptor->connect_port(handle, 6, &clipped_samples);      // Clipped Samples (output)
    descriptor->connect_port(handle, 7, &detected_clicks);      // Detected Clicks (output)
    descriptor->connect_port(handle, 8, &avg_spike_length);     // Average Spike Length (output)
    descriptor->connect_port(handle, 9, &avg_rms_db);           // Average RMS dB (output)
    descriptor->connect_port(handle, 10, input_l);              // Input L
    descriptor->connect_port(handle, 11, input_r);              // Input R
    descriptor->connect_port(handle, 12, output_l);             // Output L
    descriptor->connect_port(handle, 13, output_r);             // Output R
    descriptor->connect_port(handle, 14, &store_settings);      // Store settings
    
    // Activate plugin
    if (descriptor->activate) {
        descriptor->activate(handle);
    }
    
    printf("Processing settings:\n");
    printf("  Gain: %.1f dB\n", gain);
    printf("  Subsonic: %d (0=off, 1=1st order, 2=2nd order)\n", (int)subsonic);
    printf("  RIAA: %s\n", riaa_enable ? "enabled" : "disabled");
    printf("  Declick: %s\n", declick_enable ? "enabled" : "disabled");
    if (declick_enable) {
        printf("  Spike threshold: %.0f\n", spike_threshold);
        printf("  Spike width: %.1f ms\n", spike_width);
    }
    printf("\nProcessing...\n");
    
    // Process audio
    sf_count_t frames_read;
    sf_count_t total_frames = 0;
    
    while ((frames_read = sf_readf_float(sf_in, interleaved, BUFFER_SIZE)) > 0) {
        // Deinterleave
        for (sf_count_t i = 0; i < frames_read; i++) {
            input_l[i] = interleaved[i * 2];
            input_r[i] = interleaved[i * 2 + 1];
        }
        
        // Process
        descriptor->run(handle, frames_read);
        
        // Interleave output
        for (sf_count_t i = 0; i < frames_read; i++) {
            interleaved[i * 2] = output_l[i];
            interleaved[i * 2 + 1] = output_r[i];
        }
        
        // Write output
        sf_writef_float(sf_out, interleaved, frames_read);
        
        total_frames += frames_read;
    }
    
    printf("Processed %ld frames\n", (long)total_frames);
    printf("\n");
    printf("Results:\n");
    printf("  Clipped samples: %.0f\n", clipped_samples);
    printf("  Detected clicks: %.0f\n", detected_clicks);
    if (declick_enable && detected_clicks > 0) {
        printf("  Average spike length: %.1f samples\n", avg_spike_length);
        printf("  Average spike/background ratio: %.1f dB\n", avg_rms_db);
    }
    printf("\n");
    printf("Output: %s\n", argv[2]);
    
    // Cleanup
    if (descriptor->deactivate) {
        descriptor->deactivate(handle);
    }
    descriptor->cleanup(handle);
    
    free(input_l);
    free(input_r);
    free(output_l);
    free(output_r);
    free(interleaved);
    
    sf_close(sf_in);
    sf_close(sf_out);
    dlclose(plugin_lib);
    
    return 0;
}
