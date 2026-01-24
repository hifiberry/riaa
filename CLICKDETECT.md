# Click Detector Module

MAD (Median Absolute Deviation) based click detector for vinyl de-clicking.

## Overview

This module implements a robust, adaptive click detection algorithm that:
- Uses MAD statistics instead of fixed thresholds
- Automatically adapts to signal level
- **Integrated high-pass filter** for click emphasis (10-12 kHz)
- Resists false triggering on music transients (drums, cymbals)
- Validates click candidates by length and energy

## Files

- `clickdetect.h` - Header file with API definitions
- `clickdetect.c` - Implementation
- `biquad.h`, `biquad.c` - Biquad filter module with HPF coefficient generation
- `test_clickdetect.c` - Basic test program with synthetic clicks
- `test_hpf_effect.c` - Test demonstrating HPF effectiveness

## Algorithm

The detector uses a multi-stage approach:

1. **High-Pass Filter**:
   - Butterworth HPF at 10-12 kHz (configurable)
   - Emphasizes clicks (broadband) while attenuating music
   - Configurable order (2nd or 4th)
2. **Windowed Analysis**: Maintains a circular buffer of filtered samples
2. **MAD Calculation**: 
   - Computes median of samples in window
   - Computes MAD (median of absolute deviations from median)
   - Scores each sample: `score = |sample - median| / (MAD + ε)`
3. **Threshold Detection**: Marks samples where score > threshold
4. **Click Validation**: Groups contiguous candidates and validates:
   - Length must be ≤ max_click_length (protects drums/transients)
   - Energy must be ≥ min_energy (rejects noise)

## Configuration Parameters

### `window_size`
- Detection window half-width in samples
- Default: 0.75ms worth of samples
- Typical range: 0.5-1.0ms

### `threshold`
- MAD score threshold multiplier
- Default: 7.0
- Range: 6-10 (lower = more sensitive, higher = more conservative)

### `epsilon`
- Small value to prevent division by zero
- Default: 1e-9

### `max_click_length`
- Maximum click duration in samples
- Default: 0.75ms worth of samples
- Typical range: 0.5-1.0ms
- Prevents false detection of drum hits and snares

### `min_energy`
- Minimum total energy for valid click
- Default: 0.0 (disabled)
- Can be tuned adaptively based on signal level

### `hpf_freq`
- HPF cutoff frequency in Hz
- Default: 10000.0 Hz (10 kHz)
- Typical range: 10-12 kHz

### `hpf_order`
- HPF filter order
- Default: 2 (2nd order, one biquad stage)
- Options: 2 or 4 (4th order = two cascaded biquads)

## Usage Example

```c
#include "clickdetect.h"

// Initialize configuration for 48kHz
ClickDetectorConfig config;
clickdetect_config_init(&config, 48000);

// Optionally adjust parameters
config.threshold = 7.5;       // Slightly more conservative
config.hpf_freq = 12000.0;    // Higher cutoff
config.hpf_order = 4;         // 4th order for steeper rolloff

// Create detector (must pass sample rate for HPF coefficient generation)
ClickDetector *detector = clickdetect_create(&config, 48000);

// Process samples (raw input signal, HPF applied internally)
for (int i = 0; i < num_samples; i++) {
    int click_detected = clickdetect_process(detector, input_signal[i]);
    if (click_detected) {
        // Mark this position for repair
    }
}

// Cleanup
clickdetect_free(detector);
```

## Testing

Compile and run the test program:

```bash
gcc -Wall -O2 -o test_clickdetect test_clickdetect.c clickdetect.c -lm
./test_clickdetect
```

The test generates a sine wave with three synthetic clicks at different positions and validates detection.
### Testing with Real Audio Files

The module includes a test program for processing real audio files:

```bash
# Compile (requires libsndfile)
gcc -Wall -O2 -o test_wav_file test_wav_file.c clickdetect.c biquad.c -lm -lsndfile

# Process a WAV or FLAC file
./test_wav_file audio.wav
./test_wav_file audio.flac --threshold 6.5

# Process specific channel only
./test_wav_file stereo.wav --channel 0

# Adjust HPF settings
./test_wav_file audio.wav --hpf-freq 12000 --hpf-order 4
```

**Convenience Script**: Use `test_audio_clicks.sh` for automatic MP3 conversion:

```bash
./test_audio_clicks.sh vinyl.mp3 --threshold 7.0
./test_audio_clicks.sh vinyl.flac --channel 0 --keep-temp
```

Supported formats: WAV, FLAC, AIFF (via libsndfile), MP3 (via ffmpeg/sox conversion)
## Performance Characteristics

- **Latency**: Equal to window size (typically 0.5-1ms)
- **CPU Usage**: O(n log n) per sample due to median calculation (qsort)
- **Memory**: ~3x window size (circular buffer + 2 work arrays)

## Tuning Guidelines

### For noisy/older vinyl records:
- Lower threshold (≈ 6)
- Shorter max_click_length
- More aggressive detection

### For clean/audiophile records:
- Higher threshold (≈ 8-10)
- Conservative max_click_length
- Preserve transients better

## Next Steps

This is just the detector module. To create a complete de-clicker, you'll need:

1. **High-pass filter** (10-12 kHz) to emphasize clicks
2. **This detector** to find click positions
3. **LPC interpolation** to repair detected clicks
4. **Integration** into the RIAA plugin or separate LADSPA plugin

## References

Based on algorithm described in `declick.txt`:
- MAD-based robust detection
- Click validation by length and energy
- Designed for real-time vinyl playback (<5ms latency)
