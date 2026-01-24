# RIAA LADSPA Plugin - Examples

This document provides usage examples for the RIAA LADSPA plugin.

## Testing Plugins

Use the `test-plugin.sh` script to measure frequency response:

```bash
./test-plugin.sh <plugin_name> [parameters...] [output.txt]
```

All parameters after the plugin name are passed to the LADSPA plugin. If an argument ends with `.txt`, it's used as the output filename.

---

## RIAA Equalization Plugin

**Plugin Label:** `riaa`  
**Description:** RIAA phono preamp equalization with optional subsonic filtering

### Parameters

1. **Gain (dB)** (control, -40.0 to 40.0, default: 0.0)
   - Gain adjustment in decibels, applied after filtering

2. **Subsonic Filter** (control, 0 to 2, integer, default: 0)
   - `0` = Off (no subsonic filtering)
   - `1` = 1st order highpass at 20Hz (-6dB/octave)
   - `2` = 2nd order highpass at 20Hz (-12dB/octave, Butterworth)

3. **RIAA Enable** (control, 0 to 1, integer, default: 1)
   - `0` = Bypass RIAA equalization
   - `1` = Enable RIAA equalization

4. **Store settings** (control, 0 to 1, integer, default: 0)
   - Set to 1 to save current parameters to config file

### Examples

**No subsonic filter, 0dB gain:**
```bash
./test-plugin.sh riaa 0 0 1
# Output: riaa_response.txt
```

**With 1st order subsonic filter:**
```bash
./test-plugin.sh riaa 0 1 1 riaa_subsonic1.txt
# Gain=0dB, Subsonic=1 (1st order), RIAA=enabled
# Output: riaa_subsonic1.txt
```

**With 2nd order subsonic filter:**
```bash
./test-plugin.sh riaa 0 2 1 riaa_subsonic2.txt
# Gain=0dB, Subsonic=2 (2nd order), RIAA=enabled
# Output: riaa_subsonic2.txt
```

**Testing with gain adjustment:**
```bash
./test-plugin.sh riaa 6.0 2 1
# Gain=6.0dB boost, Subsonic=2nd order, RIAA=enabled
```

### Expected Results

**With RIAA enabled:**
- RIAA equalization curve applied (inverse of recording curve)
- Gain adjustment applied after filtering

**Without subsonic filter (mode 0):**
- Flat above ~100 Hz
- -6 dB/octave rolloff below 20 Hz
- At 10 Hz: approximately -6 dB attenuation

**With 2nd order subsonic (mode 2):**
- Flat above ~100 Hz
- -12 dB/octave rolloff below 20 Hz
- At 10 Hz: approximately -12 dB attenuation

### Manual Testing with Sox

**Test specific frequency with subsonic filter:**
```bash
sox -n -t null - synth 1.0 sine 10 trim 0.2 0.6 \
    ladspa /usr/local/lib/ladspa/riaa.so riaa 0 2 1 \
    stat 2>&1 | grep "RMS"
```

**Compare 10Hz vs 1kHz with 2nd order subsonic:**
```bash
# At 10 Hz (should be attenuated)
sox -n -t null - synth 1.0 sine 10 trim 0.2 0.6 \
    ladspa /usr/local/lib/ladspa/riaa.so riaa 0 2 1 \
    stat 2>&1 | grep "RMS amplitude"

# At 1000 Hz (should pass through)
sox -n -t null - synth 1.0 sine 1000 trim 0.2 0.6 \
    ladspa /usr/local/lib/ladspa/riaa.so riaa 0 2 1 \
    stat 2>&1 | grep "RMS amplitude"
```

---

## Sample Rate Support

The plugin supports the following sample rates:
- 44.1 kHz
- 48 kHz
- 88.2 kHz
- 96 kHz
- 176.4 kHz
- 192 kHz

The plugin will print an error and fail to instantiate if an unsupported sample rate is used.

---

## PipeWire Integration

### RIAA Plugin Configuration

Example PipeWire configuration (`~/.config/pipewire/pipewire.conf.d/riaa-filter.conf`):

```
context.modules = [
    {   name = libpipewire-module-filter-chain
        args = {
            node.description = "RIAA Phono Preamp"
            media.name       = "RIAA Phono Preamp"
            filter.graph = {
                nodes = [
                    {
                        type   = ladspa
                        name   = riaa
                        plugin = /usr/local/lib/ladspa/riaa.so
                        label  = riaa
                        control = {
                            "Gain (dB)" = 0.0
                            "Subsonic Filter" = 2.0
                            "RIAA Enable" = 1.0
                        }
                    }
                ]
            }
            audio.position = [ FL FR ]
            capture.props = {
                node.name      = "effect_input.riaa"
                media.class    = Audio/Sink
                audio.channels = 2
                audio.position = [ FL FR ]
                audio.rate     = 96000
            }
            playback.props = {
                node.name      = "effect_output.riaa"
                node.passive   = true
                audio.channels = 2
                audio.position = [ FL FR ]
                audio.rate     = 96000
            }
        }
    }
]
```

### Runtime Parameter Adjustment

Find the node ID:
```bash
pw-cli list-objects | grep -A 5 "effect_input.riaa"
```

View current parameters:
```bash
pw-cli enum-params <node_id> Props
```

Change parameters on the fly:
```bash
# Change gain
pw-cli set-param <node_id> Props '{ params = [ "riaa:Gain (dB)", 6.0 ] }'

# Enable 2nd order subsonic filter
pw-cli set-param <node_id> Props '{ params = [ "riaa:Subsonic Filter", 2.0 ] }'

# Disable subsonic filter
pw-cli set-param <node_id> Props '{ params = [ "riaa:Subsonic Filter", 0.0 ] }'
```

---

## Building and Installing

**Build all plugins:**
```bash
make
```

**Install plugin:**
```bash
sudo make install
```

**Install all:**
```bash
sudo make install-all
```

**Clean build files:**
```bash
make clean
```

---

## Plugin Information

Use `analyseplugin` to inspect plugin details:

```bash
analyseplugin /usr/local/lib/ladspa/riaa.so
```
