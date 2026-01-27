/*
 * riaa.c - RIAA Equalization LADSPA Plugin
 * 
 * Implements RIAA equalization for vinyl playback with optional subsonic filtering.
 * Includes selectable 1st order (-6dB/oct) or 2nd order (-12dB/oct) subsonic filter at 20Hz.
 * 
 * RIAA curve time constants: 3180µs, 318µs, 75µs
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ladspa.h>
#include "dsp/biquad.h"
#include "dsp/riaa_coeffs.h"
#include "dsp/riaa_calc.h"
#include "riaa_ladspa.h"
#include "dsp/decibel.h"
#include "utils/counter.h"
#include "utils/configfile.h"
#include "dsp/declick.h"
#include "utils/controls.h"
#include "dsp/samplerate.h"
#include "dsp/notch.h"

#define RIAA_GAIN                 0
#define RIAA_SUBSONIC_SEL         1
#define RIAA_ENABLE               2
#define RIAA_DECLICK_ENABLE       3
#define RIAA_SPIKE_THRESHOLD      4
#define RIAA_SPIKE_WIDTH          5
#define RIAA_NOTCH_ENABLE         6
#define RIAA_NOTCH_FREQ           7
#define RIAA_NOTCH_Q              8
#define RIAA_CLIPPED_SAMPLES      9
#define RIAA_DETECTED_CLICKS     10
#define RIAA_AVG_SPIKE_LENGTH    11
#define RIAA_AVG_RMS_DB          12
#define RIAA_INPUT_L             13
#define RIAA_INPUT_R             14
#define RIAA_OUTPUT_L            15
#define RIAA_OUTPUT_R            16
#define RIAA_STORE_SETTINGS      17

typedef struct {
    LADSPA_Data *gain;
    LADSPA_Data *subsonic_sel;
    LADSPA_Data *riaa_enable;
    LADSPA_Data *store_settings;
    LADSPA_Data *declick_enable;
    LADSPA_Data *spike_threshold;
    LADSPA_Data *spike_width;
    LADSPA_Data *notch_enable;
    LADSPA_Data *notch_freq;
    LADSPA_Data *notch_q;
    LADSPA_Data *clipped_samples;
    LADSPA_Data *detected_clicks;
    LADSPA_Data *avg_spike_length;
    LADSPA_Data *avg_rms_db;
    LADSPA_Data *input_l;
    LADSPA_Data *input_r;
    LADSPA_Data *output_l;
    LADSPA_Data *output_r;
    
    Counter clip_counter;
    Counter click_counter;
    int sample_rate_idx;
    unsigned long sample_rate;
    
    // Declick configuration
    DeclickConfig declick_config;
    
    // Accumulated declick statistics
    double total_spike_length_sum;
    double total_log_rms_sum;
    int total_rms_samples;
    
    // Default values from config file
    LADSPA_Data default_gain;
    LADSPA_Data default_subsonic_sel;
    LADSPA_Data default_riaa_enable;
    
    // RIAA processing states for left and right channels
    RIAAChannelState channel_l;
    RIAAChannelState channel_r;
    
    // Notch filter states
    BiquadCoeffs notch_coeffs;
    BiquadState notch_state_l;
    BiquadState notch_state_r;
    float last_notch_freq;
    float last_notch_q;
} RIAA;

static LADSPA_Handle instantiate_RIAA(
    const LADSPA_Descriptor *descriptor,
    unsigned long sample_rate) {
    
    RIAA *plugin = (RIAA *)malloc(sizeof(RIAA));
    if (plugin != NULL) {
        counter_init(&plugin->clip_counter);
        counter_init(&plugin->click_counter);
        
        // Store sample rate for declick processing
        plugin->sample_rate = sample_rate;
        
        // Initialize declick configuration
        declick_config_init(&plugin->declick_config);
        plugin->declick_config.threshold = 150;  // Default spike threshold
        plugin->declick_config.click_width_ms = 1.0f;  // Default 1ms
        
        // Initialize accumulated statistics
        plugin->total_spike_length_sum = 0.0;
        plugin->total_log_rms_sum = 0.0;
        plugin->total_rms_samples = 0;
        
        // Initialize notch filter state
        memset(&plugin->notch_state_l, 0, sizeof(BiquadState));
        memset(&plugin->notch_state_r, 0, sizeof(BiquadState));
        plugin->last_notch_freq = 0.0f;
        plugin->last_notch_q = 0.0f;
        
        // Load configuration from ~/.config/ladspa/riaa.ini
        PluginConfig config;
        config_init(&config);
        
        char *config_path = config_build_path("riaa");
        if (config_path) {
            config_load(config_path, &config);
            free(config_path);
        }
        
        // Set defaults from config (use port names as keys)
        plugin->default_gain = config_get_float(&config, RIAA_PORT_NAME_GAIN, 0.0f);
        plugin->default_subsonic_sel = config_get_float(&config, RIAA_PORT_NAME_SUBSONIC_FILTER, 0.0f);
        plugin->default_riaa_enable = config_get_float(&config, RIAA_PORT_NAME_ENABLE, 1.0f);
        
        // Find the sample rate index
        plugin->sample_rate_idx = get_sample_rate_index(sample_rate);
        
        if (plugin->sample_rate_idx < 0) {
            fprintf(stderr, "RIAA: Unsupported sample rate %lu Hz\n", sample_rate);
            fprintf(stderr, "RIAA: Supported rates: 44.1, 48, 88.2, 96, 176.4, 192 kHz\n");
            free(plugin);
            return NULL;
        }
        
        // Initialize RIAA processing for both channels
        riaa_channel_init(&plugin->channel_l, plugin->sample_rate_idx);
        riaa_channel_init(&plugin->channel_r, plugin->sample_rate_idx);
        
        fprintf(stderr, "RIAA: Initialized at %lu Hz (index %d)\n", 
                sample_rate, plugin->sample_rate_idx);
    }
    return (LADSPA_Handle)plugin;
}

static void connect_port_RIAA(
    LADSPA_Handle instance,
    unsigned long port,
    LADSPA_Data *data) {
    
    RIAA *plugin = (RIAA *)instance;
    
    switch (port) {
        case RIAA_GAIN:
            plugin->gain = data;
            break;
        case RIAA_SUBSONIC_SEL:
            plugin->subsonic_sel = data;
            break;
        case RIAA_ENABLE:
            plugin->riaa_enable = data;
            break;
        case RIAA_STORE_SETTINGS:
            plugin->store_settings = data;
            break;
        case RIAA_DECLICK_ENABLE:
            plugin->declick_enable = data;
            break;
        case RIAA_SPIKE_THRESHOLD:
            plugin->spike_threshold = data;
            break;
        case RIAA_SPIKE_WIDTH:
            plugin->spike_width = data;
            break;
        case RIAA_NOTCH_ENABLE:
            plugin->notch_enable = data;
            break;
        case RIAA_NOTCH_FREQ:
            plugin->notch_freq = data;
            break;
        case RIAA_NOTCH_Q:
            plugin->notch_q = data;
            break;
        case RIAA_CLIPPED_SAMPLES:
            plugin->clipped_samples = data;
            break;
        case RIAA_DETECTED_CLICKS:
            plugin->detected_clicks = data;
            break;
        case RIAA_AVG_SPIKE_LENGTH:
            plugin->avg_spike_length = data;
            break;
        case RIAA_AVG_RMS_DB:
            plugin->avg_rms_db = data;
            break;
        case RIAA_INPUT_L:
            plugin->input_l = data;
            break;
        case RIAA_INPUT_R:
            plugin->input_r = data;
            break;
        case RIAA_OUTPUT_L:
            plugin->output_l = data;
            break;
        case RIAA_OUTPUT_R:
            plugin->output_r = data;
            break;
    }
}

static void activate_RIAA(LADSPA_Handle instance) {
    RIAA *plugin = (RIAA *)instance;
    
    // Apply default values from config to control ports
    if (plugin->gain) {
        *(plugin->gain) = plugin->default_gain;
    }
    if (plugin->subsonic_sel) {
        *(plugin->subsonic_sel) = plugin->default_subsonic_sel;
    }
    if (plugin->riaa_enable) {
        *(plugin->riaa_enable) = plugin->default_riaa_enable;
    }
    
    // Reset RIAA processing states
    riaa_channel_reset(&plugin->channel_l);
    riaa_channel_reset(&plugin->channel_r);
    
    // Reset counters
    counter_reset(&plugin->clip_counter);
    counter_reset(&plugin->click_counter);
    
    // Reset statistics accumulators
    plugin->total_spike_length_sum = 0.0;
    plugin->total_log_rms_sum = 0.0;
    plugin->total_rms_samples = 0;
}

static void run_RIAA(
    LADSPA_Handle instance,
    unsigned long sample_count) {
    
    RIAA *plugin = (RIAA *)instance;
    LADSPA_Data *input_l = plugin->input_l;
    LADSPA_Data *input_r = plugin->input_r;
    LADSPA_Data *output_l = plugin->output_l;
    LADSPA_Data *output_r = plugin->output_r;
    
    // Safely read control ports with NULL checks and defaults
    LADSPA_Data gain_db = plugin->gain ? *(plugin->gain) : plugin->default_gain;
    LADSPA_Data gain = db_to_voltage(gain_db);
    int subsonic_sel = plugin->subsonic_sel ? (int)(*(plugin->subsonic_sel) + 0.5f) : (int)plugin->default_subsonic_sel;
    int riaa_enable = plugin->riaa_enable ? (int)(*(plugin->riaa_enable) + 0.5f) : (int)plugin->default_riaa_enable;
    int declick_enable = plugin->declick_enable ? (int)(*(plugin->declick_enable) + 0.5f) : 0;
    int notch_enable = plugin->notch_enable ? (int)(*(plugin->notch_enable) + 0.5f) : 0;
    float notch_freq = plugin->notch_freq ? *(plugin->notch_freq) : 50.0f;
    float notch_q = plugin->notch_q ? *(plugin->notch_q) : 10.0f;
    
    // Update notch filter coefficients if parameters changed
    if (notch_enable && (notch_freq != plugin->last_notch_freq || notch_q != plugin->last_notch_q)) {
        calculate_notch_coeffs(&plugin->notch_coeffs, notch_freq, notch_q, plugin->sample_rate);
        plugin->last_notch_freq = notch_freq;
        plugin->last_notch_q = notch_q;
    }
    
    // Update declick configuration from control ports
    if (plugin->spike_threshold) {
        // Convert from dB (0-40) to raw threshold value (1-900)
        // threshold = 10^(dB/20) gives voltage ratio
        // Scale: 0 dB -> 1, 20 dB -> 100, 40 dB -> 900
        float threshold_db = *(plugin->spike_threshold);
        float voltage_ratio = powf(10.0f, threshold_db / 20.0f);
        // Map to declick range: multiply by ~9 to get 0dB->1, 20dB->100, 40dB->900
        plugin->declick_config.threshold = (int)(voltage_ratio * 9.0f + 0.5f);
        // Clamp to valid range
        if (plugin->declick_config.threshold < 1) plugin->declick_config.threshold = 1;
        if (plugin->declick_config.threshold > 900) plugin->declick_config.threshold = 900;
    }
    if (plugin->spike_width) {
        plugin->declick_config.click_width_ms = *(plugin->spike_width);
    }
    
    // Copy input to output buffers first
    memcpy(output_l, input_l, sample_count * sizeof(LADSPA_Data));
    memcpy(output_r, input_r, sample_count * sizeof(LADSPA_Data));
    
    // Apply declick if enabled (process before RIAA)
    DeclickStats stats_l = {0};
    DeclickStats stats_r = {0};
    
    if (declick_enable && sample_count >= 4096) {
        int clicks_l = declick_process(output_l, sample_count, &plugin->declick_config, plugin->sample_rate, &stats_l);
        int clicks_r = declick_process(output_r, sample_count, &plugin->declick_config, plugin->sample_rate, &stats_r);
        
        // Accumulate click counts
        for (int c = 0; c < clicks_l + clicks_r; c++) {
            counter_increment(&plugin->click_counter);
        }
        
        // Accumulate statistics from both channels
        if (stats_l.click_count > 0) {
            plugin->total_spike_length_sum += stats_l.avg_spike_length * stats_l.click_count;
        }
        if (stats_r.click_count > 0) {
            plugin->total_spike_length_sum += stats_r.avg_spike_length * stats_r.click_count;
        }
        
        // Accumulate spike-to-background ratio (already in dB as power ratio)
        // Convert from dB to linear, accumulate, then convert back
        // Note: we accumulate even if avg_rms_db is 0.0, as long as clicks were detected
        if (stats_l.click_count > 0) {
            double linear_ratio = (stats_l.avg_rms_db > 0.0) ? pow(10.0, stats_l.avg_rms_db / 10.0) : 1.0;
            plugin->total_log_rms_sum += linear_ratio;
            plugin->total_rms_samples++;
        }
        if (stats_r.click_count > 0) {
            double linear_ratio = (stats_r.avg_rms_db > 0.0) ? pow(10.0, stats_r.avg_rms_db / 10.0) : 1.0;
            plugin->total_log_rms_sum += linear_ratio;
            plugin->total_rms_samples++;
        }
    }
    
    for (unsigned long i = 0; i < sample_count; i++) {
        LADSPA_Data y_l = output_l[i];
        LADSPA_Data y_r = output_r[i];
        
        // Apply RIAA processing (subsonic + RIAA EQ)
        y_l = riaa_process_sample(&plugin->channel_l, y_l, subsonic_sel, riaa_enable);
        y_r = riaa_process_sample(&plugin->channel_r, y_r, subsonic_sel, riaa_enable);
        
        // Apply notch filter if enabled
        if (notch_enable) {
            y_l = process_biquad(&plugin->notch_coeffs, &plugin->notch_state_l, y_l);
            y_r = process_biquad(&plugin->notch_coeffs, &plugin->notch_state_r, y_r);
        }
        
        // Apply gain at the end - both channels
        output_l[i] = y_l * gain;
        output_r[i] = y_r * gain;
        
        // Detect clipping on outputs
        if (output_l[i] > 1.0f || output_l[i] < -1.0f) {
            counter_increment(&plugin->clip_counter);
        }
        if (output_r[i] > 1.0f || output_r[i] < -1.0f) {
            counter_increment(&plugin->clip_counter);
        }
    }
    
    // Update output ports
    if (plugin->clipped_samples) {
        *(plugin->clipped_samples) = (LADSPA_Data)counter_get(&plugin->clip_counter);
    }
    if (plugin->detected_clicks) {
        *(plugin->detected_clicks) = (LADSPA_Data)counter_get(&plugin->click_counter);
    }
    
    // Update statistics output ports
    unsigned long total_clicks = counter_get(&plugin->click_counter);
    if (plugin->avg_spike_length) {
        if (total_clicks > 0) {
            *(plugin->avg_spike_length) = (LADSPA_Data)(plugin->total_spike_length_sum / total_clicks);
        } else {
            *(plugin->avg_spike_length) = 0.0f;
        }
    }
    if (plugin->avg_rms_db) {
        if (plugin->total_rms_samples > 0) {
            // Average the linear ratios, then convert back to dB
            double avg_ratio_linear = plugin->total_log_rms_sum / plugin->total_rms_samples;
            *(plugin->avg_rms_db) = (LADSPA_Data)(10.0 * log10(avg_ratio_linear));  // 10*log10 for power ratio
        } else {
            *(plugin->avg_rms_db) = 0.0f;
        }
    }
    
    // Check if settings should be stored
    if (plugin->store_settings && *(plugin->store_settings) != 0.0f) {
        // Build config and save current settings
        PluginConfig config;
        config_init(&config);
        
        // Convert current settings to strings and add to config
        char value_str[MAX_VALUE_LENGTH];
        
        snprintf(value_str, sizeof(value_str), "%.1f", gain_db);
        config_set(&config, RIAA_PORT_NAME_GAIN, value_str);
        
        snprintf(value_str, sizeof(value_str), "%d", subsonic_sel);
        config_set(&config, RIAA_PORT_NAME_SUBSONIC_FILTER, value_str);
        
        snprintf(value_str, sizeof(value_str), "%d", riaa_enable);
        config_set(&config, RIAA_PORT_NAME_ENABLE, value_str);
        
        // Save to config file
        char *config_path = config_build_path("riaa");
        if (config_path) {
            config_save(config_path, &config);
            free(config_path);
        }
        
        // Reset store_settings to 0 after saving
        *(plugin->store_settings) = 0.0f;
    }
}

static void cleanup_RIAA(LADSPA_Handle instance) {
    free(instance);
}

static LADSPA_Descriptor *g_riaa_descriptor = NULL;

const LADSPA_Descriptor *ladspa_descriptor(unsigned long index) {
    if (index != 0) {
        return NULL;
    }
    
    if (g_riaa_descriptor == NULL) {
        g_riaa_descriptor = (LADSPA_Descriptor *)malloc(sizeof(LADSPA_Descriptor));
        
        if (g_riaa_descriptor != NULL) {
            g_riaa_descriptor->UniqueID = 6839;
            g_riaa_descriptor->Label = strdup("riaa");
            g_riaa_descriptor->Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE;
            g_riaa_descriptor->Name = strdup("RIAA Equalization with Subsonic Filter (Stereo)");
            g_riaa_descriptor->Maker = strdup("HiFiBerry");
            g_riaa_descriptor->Copyright = strdup("MIT");
            g_riaa_descriptor->PortCount = 18;
            
            LADSPA_PortDescriptor *port_descriptors = 
                (LADSPA_PortDescriptor *)calloc(18, sizeof(LADSPA_PortDescriptor));
            port_descriptors[RIAA_GAIN] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_SUBSONIC_SEL] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_ENABLE] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_STORE_SETTINGS] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_DECLICK_ENABLE] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_SPIKE_THRESHOLD] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_SPIKE_WIDTH] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_NOTCH_ENABLE] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_NOTCH_FREQ] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_NOTCH_Q] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_CLIPPED_SAMPLES] = 
                LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_DETECTED_CLICKS] = 
                LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_AVG_SPIKE_LENGTH] = 
                LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_AVG_RMS_DB] = 
                LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_INPUT_L] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
            port_descriptors[RIAA_INPUT_R] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
            port_descriptors[RIAA_OUTPUT_L] = 
                LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
            port_descriptors[RIAA_OUTPUT_R] = 
                LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
            port_descriptors[RIAA_GAIN] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_SUBSONIC_SEL] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_ENABLE] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_STORE_SETTINGS] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_DECLICK_ENABLE] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_SPIKE_THRESHOLD] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_SPIKE_WIDTH] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_CLIPPED_SAMPLES] = 
                LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_DETECTED_CLICKS] = 
                LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_AVG_SPIKE_LENGTH] = 
                LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_AVG_RMS_DB] = 
                LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
            port_descriptors[RIAA_INPUT_L] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
            port_descriptors[RIAA_INPUT_R] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
            port_descriptors[RIAA_OUTPUT_L] = 
                LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
            port_descriptors[RIAA_OUTPUT_R] = 
                LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
            g_riaa_descriptor->PortDescriptors = port_descriptors;
            
            const char **port_names = (const char **)calloc(18, sizeof(char *));
            port_names[RIAA_GAIN] = strdup(RIAA_PORT_NAME_GAIN);
            port_names[RIAA_SUBSONIC_SEL] = strdup(RIAA_PORT_NAME_SUBSONIC_FILTER);
            port_names[RIAA_ENABLE] = strdup(RIAA_PORT_NAME_ENABLE);
            port_names[RIAA_STORE_SETTINGS] = strdup(PORT_NAME_STORE_SETTINGS);
            port_names[RIAA_DECLICK_ENABLE] = strdup("Declick Enable");
            port_names[RIAA_SPIKE_THRESHOLD] = strdup("Spike Threshold (dB)");
            port_names[RIAA_SPIKE_WIDTH] = strdup("Spike Width (ms)");
            port_names[RIAA_NOTCH_ENABLE] = strdup("Notch Filter Enable");
            port_names[RIAA_NOTCH_FREQ] = strdup("Notch Frequency (Hz)");
            port_names[RIAA_NOTCH_Q] = strdup("Notch Q Factor");
            port_names[RIAA_CLIPPED_SAMPLES] = strdup(PORT_NAME_CLIPPED_SAMPLES);
            port_names[RIAA_DETECTED_CLICKS] = strdup("Detected Clicks");
            port_names[RIAA_AVG_SPIKE_LENGTH] = strdup("Average Spike Length (samples)");
            port_names[RIAA_AVG_RMS_DB] = strdup("Average Spike Ratio (dB)");
            port_names[RIAA_INPUT_L] = strdup(PORT_NAME_INPUT_L);
            port_names[RIAA_INPUT_R] = strdup(PORT_NAME_INPUT_R);
            port_names[RIAA_OUTPUT_L] = strdup(PORT_NAME_OUTPUT_L);
            port_names[RIAA_OUTPUT_R] = strdup(PORT_NAME_OUTPUT_R);
            g_riaa_descriptor->PortNames = port_names;
            
            LADSPA_PortRangeHint *port_range_hints = 
                (LADSPA_PortRangeHint *)calloc(18, sizeof(LADSPA_PortRangeHint));
            
            // Gain: -40.0 to +40.0 dB, default 0 dB (unity)
            port_range_hints[RIAA_GAIN].HintDescriptor = 
                LADSPA_HINT_BOUNDED_BELOW | 
                LADSPA_HINT_BOUNDED_ABOVE |
                LADSPA_HINT_DEFAULT_0;
            port_range_hints[RIAA_GAIN].LowerBound = -40.0f;
            port_range_hints[RIAA_GAIN].UpperBound = 40.0f;
            
            // Subsonic filter: 0 = off, 1 = 1st order, 2 = 2nd order, default = 0 (off)
            port_range_hints[RIAA_SUBSONIC_SEL].HintDescriptor = 
                LADSPA_HINT_BOUNDED_BELOW | 
                LADSPA_HINT_BOUNDED_ABOVE |
                LADSPA_HINT_INTEGER |
                LADSPA_HINT_DEFAULT_0;
            port_range_hints[RIAA_SUBSONIC_SEL].LowerBound = 0.0f;
            port_range_hints[RIAA_SUBSONIC_SEL].UpperBound = 2.0f;
            
            // RIAA enable: 0 = off, 1 = on, default = 1 (enabled)
            port_range_hints[RIAA_ENABLE].HintDescriptor = 
                LADSPA_HINT_TOGGLED |
                LADSPA_HINT_DEFAULT_1;
            port_range_hints[RIAA_ENABLE].LowerBound = 0.0f;
            port_range_hints[RIAA_ENABLE].UpperBound = 0.0f;
            
            // Store settings: 0 = no action, != 0 = save settings, default = 0 (no action)
            port_range_hints[RIAA_STORE_SETTINGS].HintDescriptor = 
                LADSPA_HINT_TOGGLED |
                LADSPA_HINT_DEFAULT_0;
            port_range_hints[RIAA_STORE_SETTINGS].LowerBound = 0.0f;
            port_range_hints[RIAA_STORE_SETTINGS].UpperBound = 0.0f;
            
            // Declick enable: 0 = off, 1 = on, default = 0 (disabled)
            port_range_hints[RIAA_DECLICK_ENABLE].HintDescriptor = 
                LADSPA_HINT_TOGGLED |
                LADSPA_HINT_DEFAULT_0;
            port_range_hints[RIAA_DECLICK_ENABLE].LowerBound = 0.0f;
            port_range_hints[RIAA_DECLICK_ENABLE].UpperBound = 0.0f;
            
            // Spike Threshold: 0 to 40 dB, default = 15 dB
            port_range_hints[RIAA_SPIKE_THRESHOLD].HintDescriptor = 
                LADSPA_HINT_BOUNDED_BELOW | 
                LADSPA_HINT_BOUNDED_ABOVE |
                LADSPA_HINT_DEFAULT_MIDDLE;
            port_range_hints[RIAA_SPIKE_THRESHOLD].LowerBound = 0.0f;
            port_range_hints[RIAA_SPIKE_THRESHOLD].UpperBound = 40.0f;
            
            // Spike Width: 0.1 to 10.0 ms, default = 1.0 ms
            port_range_hints[RIAA_SPIKE_WIDTH].HintDescriptor = 
                LADSPA_HINT_BOUNDED_BELOW | 
                LADSPA_HINT_BOUNDED_ABOVE |
                LADSPA_HINT_DEFAULT_1;
            port_range_hints[RIAA_SPIKE_WIDTH].LowerBound = 0.1f;
            port_range_hints[RIAA_SPIKE_WIDTH].UpperBound = 10.0f;
            
            // Notch Filter Enable: 0 = off, 1 = on, default = 0 (disabled)
            port_range_hints[RIAA_NOTCH_ENABLE].HintDescriptor = 
                LADSPA_HINT_TOGGLED |
                LADSPA_HINT_DEFAULT_0;
            port_range_hints[RIAA_NOTCH_ENABLE].LowerBound = 0.0f;
            port_range_hints[RIAA_NOTCH_ENABLE].UpperBound = 0.0f;
            
            // Notch Frequency: 20 to 500 Hz, default = 50 Hz
            port_range_hints[RIAA_NOTCH_FREQ].HintDescriptor = 
                LADSPA_HINT_BOUNDED_BELOW | 
                LADSPA_HINT_BOUNDED_ABOVE;
            port_range_hints[RIAA_NOTCH_FREQ].LowerBound = 20.0f;
            port_range_hints[RIAA_NOTCH_FREQ].UpperBound = 500.0f;
            
            // Notch Q: 0.5 to 50.0, default = 10.0
            port_range_hints[RIAA_NOTCH_Q].HintDescriptor = 
                LADSPA_HINT_BOUNDED_BELOW | 
                LADSPA_HINT_BOUNDED_ABOVE;
            port_range_hints[RIAA_NOTCH_Q].LowerBound = 0.5f;
            port_range_hints[RIAA_NOTCH_Q].UpperBound = 50.0f;
            
            // Clipped Samples: read-only output
            port_range_hints[RIAA_CLIPPED_SAMPLES].HintDescriptor = 0;
            
            // Detected Clicks: read-only output
            port_range_hints[RIAA_DETECTED_CLICKS].HintDescriptor = 0;
            
            // Average Spike Length: read-only output (samples)
            port_range_hints[RIAA_AVG_SPIKE_LENGTH].HintDescriptor = 0;
            
            // Average Spike Ratio: read-only output (dB) - how much spikes exceed background
            port_range_hints[RIAA_AVG_RMS_DB].HintDescriptor = 0;
            
            port_range_hints[RIAA_INPUT_L].HintDescriptor = 0;
            port_range_hints[RIAA_INPUT_R].HintDescriptor = 0;
            port_range_hints[RIAA_OUTPUT_L].HintDescriptor = 0;
            port_range_hints[RIAA_OUTPUT_R].HintDescriptor = 0;
            
            g_riaa_descriptor->PortRangeHints = port_range_hints;
            
            g_riaa_descriptor->instantiate = instantiate_RIAA;
            g_riaa_descriptor->connect_port = connect_port_RIAA;
            g_riaa_descriptor->activate = activate_RIAA;
            g_riaa_descriptor->run = run_RIAA;
            g_riaa_descriptor->run_adding = NULL;
            g_riaa_descriptor->set_run_adding_gain = NULL;
            g_riaa_descriptor->deactivate = NULL;
            g_riaa_descriptor->cleanup = cleanup_RIAA;
        }
    }
    
    return g_riaa_descriptor;
}

void _cleanup(void) __attribute__((destructor));
void _cleanup(void) {
    if (g_riaa_descriptor != NULL) {
        free((char *)g_riaa_descriptor->Label);
        free((char *)g_riaa_descriptor->Name);
        free((char *)g_riaa_descriptor->Maker);
        free((char *)g_riaa_descriptor->Copyright);
        free((LADSPA_PortDescriptor *)g_riaa_descriptor->PortDescriptors);
        for (unsigned long i = 0; i < g_riaa_descriptor->PortCount; i++) {
            free((char *)g_riaa_descriptor->PortNames[i]);
        }
        free((char **)g_riaa_descriptor->PortNames);
        free((LADSPA_PortRangeHint *)g_riaa_descriptor->PortRangeHints);
        free(g_riaa_descriptor);
        g_riaa_descriptor = NULL;
    }
}
