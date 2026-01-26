/*
 * declick.c - Click removal for audio
 * 
 * Inspired by Audacity's Click Removal effect
 * Original source: https://github.com/audacity/audacity/tree/master/src/effects/builtin/clickremoval
 * 
 * Original Audacity code by Craig DeForest
 * Adapted for standalone C implementation
 * 
 * Algorithm:
 *   Clicks are identified as small regions of high amplitude compared
 *   to the surrounding chunk of sound. Anything sufficiently tall compared
 *   to a large (8192 sample) window around it, and sufficiently narrow,
 *   is considered to be a click and replaced with linear interpolation.
 */

#include "declick.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define WINDOW_SIZE 8192
#define MIN_BUFFER_SIZE 4096

void declick_config_init(DeclickConfig* config)
{
    config->threshold = 200;           // Default threshold
    config->click_width_ms = 0.5f;     // Default 0.5ms (20 samples @ 44.1kHz)
}

bool declick_process(float* buffer, size_t len, const DeclickConfig* config, unsigned long sample_rate)
{
    if (len < MIN_BUFFER_SIZE || config->threshold == 0 || config->click_width_ms <= 0.0f) {
        return false;
    }

    // Convert click width from milliseconds to samples
    int click_width = (int)(config->click_width_ms * sample_rate / 1000.0f);
    if (click_width < 1) {
        click_width = 1;
    }

    bool result = false;
    size_t i, j;
    int left = 0;
    int sep = 2049;
    int s2 = sep / 2;
    
    float threshold_level = (float)config->threshold;
    
    // Allocate working buffers
    float* ms_seq = (float*)malloc(len * sizeof(float));
    float* b2 = (float*)malloc(len * sizeof(float));
    
    if (!ms_seq || !b2) {
        free(ms_seq);
        free(b2);
        return false;
    }
    
    // Compute squared values
    for (i = 0; i < len; i++) {
        b2[i] = buffer[i] * buffer[i];
    }
    
    // Initialize RMS sequence with squared values
    for (i = 0; i < len; i++) {
        ms_seq[i] = b2[i];
    }
    
    // Shortcut for RMS - multiple passes through b2, accumulating as we go
    // This efficiently computes a moving average using powers of 2
    for (i = 1; (int)i < sep; i *= 2) {
        for (j = 0; j < len - i; j++) {
            ms_seq[j] += ms_seq[j + i];
        }
    }
    
    // Cheat by truncating sep to next-lower power of two
    sep = i;
    
    // Normalize by window size to get mean square
    for (i = 0; i < len - sep; i++) {
        ms_seq[i] /= sep;
    }
    
    // Process with varying window widths
    // wrc is reciprocal chosen so that integer roundoff doesn't cause issues
    int wrc;
    for (wrc = click_width / 4; wrc >= 1; wrc /= 2) {
        int ww = click_width / wrc;
        
        for (i = 0; i < len - sep; i++) {
            float msw = 0;
            
            // Compute mean square in narrow window
            for (j = 0; (int)j < ww; j++) {
                msw += b2[i + s2 + j];
            }
            msw /= ww;
            
            // Check if this might be a click (narrow peak exceeds threshold)
            if (msw >= threshold_level * ms_seq[i] / 10.0f) {
                if (left == 0) {
                    // Start of potential click
                    left = i + s2;
                }
            } else {
                if (left != 0 && ((int)i - left + s2) <= ww * 2) {
                    // End of click - replace with linear interpolation
                    float lv = buffer[left];
                    float rv = buffer[i + ww + s2];
                    int click_len = i + ww + s2 - left;
                    
                    for (j = left; j < i + ww + s2; j++) {
                        result = true;
                        buffer[j] = (rv * (j - left) + lv * (i + ww + s2 - j)) 
                                    / (float)click_len;
                        b2[j] = buffer[j] * buffer[j];
                    }
                    left = 0;
                } else if (left != 0) {
                    // False alarm - reset
                    left = 0;
                }
            }
        }
    }
    
    free(ms_seq);
    free(b2);
    
    return result;
}
