# RIAA LADSPA Plugin

RIAA phono preamp equalization LADSPA plugin from HiFiBerry.

Phono preamp equalization for vinyl playback (stereo) with optional subsonic filtering.

## Features

- **RIAA Equalization**: Accurate RIAA curve implementation for vinyl playback
- **Subsonic Filter**: Optional 1st or 2nd order highpass filter at 20Hz
- **Gain Control**: Adjustable gain from -40dB to +40dB
- **Click Detection**: Real-time click detection using MAD (Median Absolute Deviation) algorithm
- **Configuration File Support**: Save and load default settings
- **Clipping Detection**: Monitors and counts clipped samples
- **Multiple Sample Rates**: 44.1, 48, 88.2, 96, 176.4, 192 kHz

## Plugin Details

- **Plugin ID**: 6839
- **Label**: `riaa`
- **Ports**: 4 control inputs, 2 control outputs, 2 audio inputs, 2 audio outputs

### Parameters

- **Gain (dB)**: -40.0 to +40.0, default: 0.0
- **Subsonic Filter**: 0 = off, 1 = 1st order, 2 = 2nd order, default: 0
- **RIAA Enable**: 0 = bypass, 1 = enabled, default: 1
- **Store settings**: 0 = no action, 1 = save current settings to config file

### Output Ports

- **Clipped Samples**: Total count of clipped samples (exceeding ±1.0)
- **Detected Clicks**: Total count of vinyl clicks/pops detected

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

See [EXAMPLES.md](EXAMPLES.md) for detailed usage examples.

## Usage Examples

### SoX (Sound eXchange)

Process audio files with sox:
```bash
# Basic RIAA with default settings (0dB gain, no subsonic filter)
sox input.wav output.wav ladspa /usr/local/lib/ladspa/riaa.so riaa 0 0 1

# With gain adjustment and 2nd order subsonic filter
sox vinyl.wav processed.wav ladspa /usr/local/lib/ladspa/riaa.so riaa 6.0 2 1
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

See [EXAMPLES.md](EXAMPLES.md) for more detailed usage examples and PipeWire runtime parameter adjustment.

## Project Structure

- `riaa.h`, `riaa.c` - RIAA equalization plugin implementation
- `biquad.h` - Biquad filter structures and processing
- `decibel.h`, `decibel.c` - Decibel/voltage conversion utilities
- `counter.h`, `counter.c` - 64-bit counter for clipping detection
- `configfile.h`, `configfile.c` - Configuration file support (INI format)
- `ini.h`, `ini.c` - INI file parser (inih library)
- `samplerate.h` - Sample rate definitions and utilities
- `test-plugin.py` - Frequency response testing script

## License

MIT License - See individual files for copyright information.

**Maker**: HiFiBerry

