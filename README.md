# RIAA LADSPA Plugin

RIAA phono preamp equalization LADSPA plugin from HiFiBerry.

Phono preamp equalization for vinyl playback (stereo) with optional subsonic filtering.

## Features

- **RIAA Equalization**: Accurate RIAA curve implementation for vinyl playback
- **Subsonic Filter**: Optional 1st or 2nd order highpass filter at 20Hz
- **Gain Control**: Adjustable gain from -40dB to +40dB
- **Declick**: Optional click/pop removal inspired by Audacity's algorithm
- **Configuration File Support**: Save and load default settings
- **Clipping Detection**: Monitors and counts clipped samples
- **Multiple Sample Rates**: 44.1, 48, 88.2, 96, 176.4, 192 kHz

## Plugin Details

- **Plugin ID**: 6839
- **Label**: `riaa`
- **Ports**: 7 control inputs, 2 control outputs, 2 audio inputs, 2 audio outputs

### Parameters

- **Gain (dB)**: -40.0 to +40.0, default: 0.0
- **Subsonic Filter**: 0 = off, 1 = 1st order, 2 = 2nd order, default: 0
- **RIAA Enable**: 0 = bypass, 1 = enabled, default: 1
- **Store settings**: 0 = no action, 1 = save current settings to config file
- **Declick Enable**: 0 = off, 1 = on, default: 0
- **Spike RMS Threshold**: 1 to 900, default: 150
- **Spike Width (ms)**: 0.1 to 10.0 ms, default: 1.0

### Output Ports

- **Clipped Samples**: Total count of clipped samples (exceeding ±1.0)
- **Detected Clicks**: Total count of detected and removed clicks

## Prerequisites

Install the LADSPA SDK:

```bash
sudo apt update
sudo apt install ladspa-sdk
```

## Building and Installation

**Build the plugin:**
```bash
make
```

**Install:**
```bash
sudo make install
```

The plugin is installed to `/usr/local/lib/ladspa/`.

## Configuration

The plugin supports loading default parameter values from `~/.config/ladspa/riaa.ini`.

**Example configuration file:**
```ini
# RIAA Plugin Configuration
# Use control port names as keys

Gain (dB) = 6.0
Subsonic Filter = 2
RIAA Enable = yes    # Also supports: yes/no, true/false (case-insensitive)
```

**Boolean Values:** For enable/disable parameters, you can use:
- `yes`, `YES`, `Yes`, `true`, `TRUE`, `True`, `1` → 1.0
- `no`, `NO`, `No`, `false`, `FALSE`, `False`, `0` → 0.0

**Saving Settings:** The plugin includes a "Store settings" control port. When set to non-zero (e.g., 1), it automatically saves the current parameter values to the config file. The control resets to 0 after saving.

Configuration files are optional. If not present, the plugin uses built-in defaults.

## Testing

**Analyze plugin information:**
```bash
analyseplugin /usr/local/lib/ladspa/riaa.so
```

**Test frequency response:**
```bash
./test-plugin.py riaa 0 0 1    # No subsonic filter, 0dB gain
./test-plugin.py riaa 0 2 1    # With 2nd order subsonic, 0dB gain
```

See [TEST-PLUGIN.md](TEST-PLUGIN.md) for detailed testing examples.

## Usage Examples

### Command-Line Utility

The `riaa_process` utility provides a convenient way to process audio files and see the output statistics (clipped samples and detected clicks):

```bash
# Basic usage with default settings (0dB gain, no subsonic, RIAA enabled, declick off)
./riaa_process input.wav output.wav

# With custom parameters
./riaa_process input.wav output.wav [gain] [subsonic] [riaa_enable] [declick_enable] [spike_threshold] [spike_width]

# Examples:
# Process with 6dB gain and 2nd order subsonic filter
./riaa_process vinyl.wav processed.wav 6.0 2 1 0

# Process with declick enabled
./riaa_process noisy.wav clean.wav 0 2 1 1 150 1.0
```

**Parameters:**
- `gain`: -40.0 to +40.0 dB (default: 0.0)
- `subsonic`: 0=off, 1=1st order, 2=2nd order (default: 0)
- `riaa_enable`: 0=bypass, 1=enabled (default: 1)
- `declick_enable`: 0=off, 1=on (default: 0)
- `spike_threshold`: 1 to 900 (default: 150)
- `spike_width`: 0.1 to 10.0 ms (default: 1.0)

**Output Example:**
```
Input: vinyl.wav
  Sample rate: 44100 Hz
  Channels: 2
  Frames: 2205000

Plugin: RIAA Phono Preamp
Label: riaa

Processing settings:
  Gain: 6.0 dB
  Subsonic: 2 (0=off, 1=1st order, 2=2nd order)
  RIAA: enabled
  Declick: enabled
  Spike threshold: 150
  Spike width: 1.0 ms

Processing...
Processed 2205000 frames

Results:
  Clipped samples: 0
  Detected clicks: 42

Output: processed.wav
```

**Note:** The utility requires libsndfile for WAV file handling. Install with:
```bash
sudo apt install libsndfile1-dev
```

### SoX (Sound eXchange)

Process audio files with sox:
```bash
# Basic RIAA with default settings (0dB gain, no subsonic filter)
sox input.wav output.wav ladspa /usr/local/lib/ladspa/riaa.so riaa 0 0 1

# With gain adjustment and 2nd order subsonic filter
sox vinyl.wav processed.wav ladspa /usr/local/lib/ladspa/riaa.so riaa 6.0 2 1
```

### Ecasound

Ecasound is a powerful command-line audio processing tool that supports LADSPA plugins.

**Basic RIAA processing:**
```bash
# Process a file with default settings
# Syntax: -el:plugin_label,param1,param2,param3
ecasound -i input.wav -o output.wav -el:riaa,0,0,1

# With 6dB gain and 2nd order subsonic filter
ecasound -i vinyl.wav -o processed.wav -el:riaa,6.0,2,1
```

**Real-time processing:**
```bash
# Process from input device to output device
ecasound -i alsa,default -o alsa,default -el:riaa,0,2,1

# With specific ALSA devices
ecasound -i alsa,hw:1,0 -o alsa,hw:0,0 -el:riaa,6.0,2,1
```

**Chain multiple effects:**
```bash
# RIAA followed by normalization
ecasound -i input.wav -o output.wav \
  -el:riaa,0,2,1 \
  -eadb:-3

# RIAA with additional EQ
ecasound -i input.wav -o output.wav \
  -el:riaa,6.0,2,1 \
  -eli:150,1,0.5
```

**Syntax explanation:**
- `-el:riaa,param1,param2,param3`
  - `riaa`: Plugin label
  - `param1`: Gain (dB), range: -40.0 to +40.0
  - `param2`: Subsonic Filter (0=off, 1=1st order, 2=2nd order)
  - `param3`: RIAA Enable (0=bypass, 1=enabled)

**Note:** For Ecasound to find the plugin, ensure it's installed in a standard LADSPA path or set the `LADSPA_PATH` environment variable:
```bash
export LADSPA_PATH=/usr/local/lib/ladspa:$LADSPA_PATH
```

### PipeWire Integration

Example configuration file `~/.config/pipewire/pipewire.conf.d/riaa-filter.conf`:

```
context.modules = [
    { name = libpipewire-module-filter-chain
      args = {
        node.description = "RIAA Phono Preamp"
        filter.graph = {
          nodes = [
            { type = ladspa
              plugin = /usr/local/lib/ladspa/riaa.so
              label = riaa
              control = {
                "Gain (dB)" = 0.0
                "Subsonic Filter" = 2.0
                "RIAA Enable" = 1.0
              }
            }
          ]
        }
        capture.props = {
          node.name = "effect_input.riaa"
          media.class = Audio/Sink
        }
      }
    }
]
```

### Audacity and Other LADSPA Hosts

Load the plugin by label `riaa` from the LADSPA effects menu.

See [TEST-PLUGIN.md](TEST-PLUGIN.md) for detailed testing examples and frequency response testing.

## License

MIT License - See [LICENSE](LICENSE) file for details.

**Maker**: HiFiBerry

