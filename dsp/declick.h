/*
 * declick.h - Click removal for audio
 * 
 * Inspired by Audacity's Click Removal effect
 * Original source: https://github.com/audacity/audacity/tree/master/src/effects/builtin/clickremoval
 * 
 * Adapted for standalone C implementation
 */

#ifndef DECLICK_H
#define DECLICK_H

#include <stddef.h>
#include <stdbool.h>

// Click removal configuration
typedef struct {
    int threshold;         // Threshold level (1-900, default: 200)
    float click_width_ms;  // Maximum click width in milliseconds (default: 0.5ms)
} DeclickConfig;

/*
 * Remove clicks from an audio buffer
 * 
 * Parameters:
 *   buffer: Audio samples to process (modified in place)
 *   len: Number of samples in buffer (must be >= 4096)
 *   config: Click removal configuration
 *   sample_rate: Sample rate in Hz (e.g., 44100, 48000)
 * 
 * Returns:
 *   Number of clicks detected and removed (0 if none)
 * 
 * Algorithm:
 *   - Identifies clicks as narrow regions of high amplitude compared to surrounding audio
 *   - Uses a large window (8192 samples) to compute local RMS
 *   - Clicks are detected when peak exceeds threshold * RMS
 *   - Detected clicks are replaced with linear interpolation
 */
int declick_process(float* buffer, size_t len, const DeclickConfig* config, unsigned long sample_rate);

/*
 * Initialize configuration with default values
 */
void declick_config_init(DeclickConfig* config);

#endif // DECLICK_H
