/*
 * riaa.h - RIAA LADSPA Plugin definitions
 * 
 * Plugin-specific port names and constants
 */

#ifndef RIAA_PLUGIN_H
#define RIAA_PLUGIN_H

// RIAA-specific control port name constants
#define RIAA_PORT_NAME_GAIN            "Gain (dB)"
#define RIAA_PORT_NAME_SUBSONIC_FILTER "Subsonic Filter"
#define RIAA_PORT_NAME_ENABLE          "RIAA Enable"
#define RIAA_PORT_NAME_DECLICK_ENABLE  "Declick Enable"
#define RIAA_PORT_NAME_SPIKE_THRESHOLD "Spike Threshold (dB)"
#define RIAA_PORT_NAME_SPIKE_WIDTH     "Spike Width (ms)"
#define RIAA_PORT_NAME_NOTCH_ENABLE    "Notch Filter Enable"
#define RIAA_PORT_NAME_NOTCH_FREQ      "Notch Frequency (Hz)"
#define RIAA_PORT_NAME_NOTCH_Q         "Notch Q Factor"

#endif // RIAA_PLUGIN_H
