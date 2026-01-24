/*
 * test_wav_file.c - Test click detector on real audio files
 * 
 * Reads WAV/FLAC files using libsndfile and processes them through
 * the click detector, reporting detected clicks with timestamps.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sndfile.h>
#include "clickdetect.h"

void print_usage(const char *prog) {
    printf("Usage: %s <audio_file> [options]\n", prog);
    printf("\nOptions:\n");
    printf("  --threshold <value>    MAD threshold (default: 7.0, range: 6-10)\n");
    printf("  --hpf-freq <freq>      HPF frequency in Hz (default: 10000)\n");
    printf("  --hpf-order <order>    HPF order, 2 or 4 (default: 2)\n");
    printf("  --channel <0|1>        Process specific channel (default: both)\n");
    printf("\nSupported formats: WAV, FLAC, AIFF, and other formats via libsndfile\n");
    printf("Note: For MP3 files, convert to WAV first using: ffmpeg -i input.mp3 output.wav\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *filename = argv[1];
    LADSPA_Data threshold = 7.0f;
    LADSPA_Data hpf_freq = 10000.0f;
    int hpf_order = 2;
    int specific_channel = -1;  // -1 = process all channels
    
    // Parse command line options
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--threshold") == 0 && i + 1 < argc) {
            threshold = atof(argv[++i]);
        } else if (strcmp(argv[i], "--hpf-freq") == 0 && i + 1 < argc) {
            hpf_freq = atof(argv[++i]);
        } else if (strcmp(argv[i], "--hpf-order") == 0 && i + 1 < argc) {
            hpf_order = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--channel") == 0 && i + 1 < argc) {
            specific_channel = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    // Open audio file
    SF_INFO sfinfo;
    memset(&sfinfo, 0, sizeof(sfinfo));
    SNDFILE *infile = sf_open(filename, SFM_READ, &sfinfo);
    
    if (!infile) {
        fprintf(stderr, "Error: Could not open file '%s'\n", filename);
        fprintf(stderr, "%s\n", sf_strerror(NULL));
        return 1;
    }
    
    printf("File: %s\n", filename);
    printf("Sample rate: %d Hz\n", sfinfo.samplerate);
    printf("Channels: %d\n", sfinfo.channels);
    printf("Format: 0x%08X\n", sfinfo.format);
    printf("Frames: %ld (%.2f seconds)\n", 
           (long)sfinfo.frames, (double)sfinfo.frames / sfinfo.samplerate);
    printf("\nClick Detector Configuration:\n");
    printf("  Threshold: %.1f\n", threshold);
    printf("  HPF frequency: %.1f Hz\n", hpf_freq);
    printf("  HPF order: %d\n", hpf_order);
    if (specific_channel >= 0) {
        printf("  Processing channel: %d\n", specific_channel);
    } else {
        printf("  Processing: all channels\n");
    }
    printf("\n");
    
    // Validate channel selection
    if (specific_channel >= sfinfo.channels) {
        fprintf(stderr, "Error: Channel %d does not exist (file has %d channels)\n",
                specific_channel, sfinfo.channels);
        sf_close(infile);
        return 1;
    }
    
    // Create click detectors (one per channel to process)
    ClickDetectorConfig config;
    clickdetect_config_init(&config, sfinfo.samplerate);
    config.threshold = threshold;
    config.hpf_freq = hpf_freq;
    config.hpf_order = hpf_order;
    
    int channels_to_process = (specific_channel >= 0) ? 1 : sfinfo.channels;
    ClickDetector **detectors = malloc(channels_to_process * sizeof(ClickDetector*));
    
    for (int i = 0; i < channels_to_process; i++) {
        detectors[i] = clickdetect_create(&config, sfinfo.samplerate);
        if (!detectors[i]) {
            fprintf(stderr, "Error: Failed to create click detector\n");
            sf_close(infile);
            return 1;
        }
    }
    
    // Allocate buffer for reading
    const int BUFFER_SIZE = 8192;
    LADSPA_Data *buffer = malloc(BUFFER_SIZE * sfinfo.channels * sizeof(LADSPA_Data));
    if (!buffer) {
        fprintf(stderr, "Error: Failed to allocate buffer\n");
        sf_close(infile);
        return 1;
    }
    
    // Process audio file
    printf("Processing...\n\n");
    
    int *click_counts = calloc(channels_to_process, sizeof(int));
    sf_count_t total_frames = 0;
    sf_count_t frames_read;
    
    while ((frames_read = sf_readf_float(infile, buffer, BUFFER_SIZE)) > 0) {
        for (sf_count_t i = 0; i < frames_read; i++) {
            sf_count_t frame_number = total_frames + i;
            double time_seconds = (double)frame_number / sfinfo.samplerate;
            
            // Process each channel
            for (int ch = 0; ch < sfinfo.channels; ch++) {
                // Skip if we're only processing a specific channel
                if (specific_channel >= 0 && ch != specific_channel) {
                    continue;
                }
                
                int detector_idx = (specific_channel >= 0) ? 0 : ch;
                LADSPA_Data sample = buffer[i * sfinfo.channels + ch];
                
                int detected = clickdetect_process(detectors[detector_idx], sample);
                if (detected) {
                    click_counts[detector_idx]++;
                    printf("✓ Click detected: Channel %d, Frame %ld, Time %.3f s\n",
                           ch, (long)frame_number, time_seconds);
                }
            }
        }
        total_frames += frames_read;
    }
    
    // Summary
    printf("\n");
    printf("Summary:\n");
    printf("========\n");
    printf("Total frames processed: %ld (%.2f seconds)\n", 
           (long)total_frames, (double)total_frames / sfinfo.samplerate);
    
    if (specific_channel >= 0) {
        printf("Channel %d: %d clicks detected\n", specific_channel, click_counts[0]);
    } else {
        for (int i = 0; i < sfinfo.channels; i++) {
            printf("Channel %d: %d clicks detected\n", i, click_counts[i]);
        }
    }
    
    // Cleanup
    free(click_counts);
    free(buffer);
    for (int i = 0; i < channels_to_process; i++) {
        clickdetect_free(detectors[i]);
    }
    free(detectors);
    sf_close(infile);
    
    return 0;
}
