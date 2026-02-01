# PipeWire Configuration for RIAA Phono Preamp

This directory contains PipeWire and WirePlumber configuration files for automatic RIAA phono preamp processing.

## Files

- `90-riaa.conf` - PipeWire filter chain configuration that loads the RIAA LADSPA plugin
- `99-riaa-autoconnect.conf` - WirePlumber configuration for automatic audio routing

## Installation

### Manual Installation

```bash
# Copy PipeWire configuration
sudo cp 90-riaa.conf /etc/pipewire/pipewire.conf.d/

# Copy WirePlumber configuration
sudo cp 99-riaa-autoconnect.conf /etc/xdg/wireplumber/wireplumber.conf.d/

# Restart audio services
systemctl --user restart pipewire wireplumber
```

### Package Installation

The Debian package automatically installs these files to the correct locations.

## How It Works

### Audio Routing

1. **Input**: ALSA capture devices (your turntable) automatically connect to the RIAA filter
2. **Processing**: Audio is processed through the RIAA phono preamp equalization
3. **Output**: Processed audio automatically connects to your default speakers

### Filter Chain

The RIAA filter provides:
- RIAA equalization curve (3180µs, 318µs, 75µs time constants)
- Optional subsonic filtering (1st or 2nd order at 20Hz)
- Adjustable gain control (-40dB to +40dB)
- Optional declick for vinyl pop/click removal
- Optional notch filter for mains hum removal (use as last resort)

### Configuration

Default parameters can be set in `~/.state/ladspa/riaa.ini`:

```ini
# RIAA Plugin Configuration
Gain (dB) = 0.0
Subsonic Filter = 2
RIAA Enable = 1
```

Boolean values support: yes/no, true/false, 1/0 (case-insensitive)

### Runtime Control

While the filter is running, you can adjust parameters using `pw-cli`:

```bash
# List all nodes to find the RIAA node ID
pw-cli list-objects

# Set parameters (replace <node-id> with the actual ID)
pw-cli set-param <node-id> Props '{ params = [ "Gain (dB)" 6.0 ] }'
pw-cli set-param <node-id> Props '{ params = [ "Subsonic Filter" 2.0 ] }'
```

## Verification

Check if the filter is running:

```bash
pw-cli list-objects | grep -i riaa
```

You should see the RIAA node listed with its connections.

## Troubleshooting

### Filter not loading

1. Check that the plugin is installed:
   ```bash
   ls /usr/lib/ladspa/riaa.so
   ```

2. Check PipeWire logs:
   ```bash
   journalctl --user -u pipewire -f
   ```

### No audio routing

1. Check WirePlumber status:
   ```bash
   systemctl --user status wireplumber
   ```

2. Manually test connections with `pw-link`:
   ```bash
   # List all ports
   pw-link -l
   
   # Connect manually
   pw-link <source-port> riaa:input_FL
   pw-link riaa:output_FL <sink-port>
   ```

## Disabling

To disable the automatic RIAA processing:

```bash
# Remove configuration files
sudo rm /etc/pipewire/pipewire.conf.d/90-riaa.conf
sudo rm /etc/xdg/wireplumber/wireplumber.conf.d/99-riaa-autoconnect.conf

# Restart services
systemctl --user restart pipewire wireplumber
```
