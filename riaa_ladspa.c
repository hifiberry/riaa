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

#define RIAA_GAIN                 0
#define RIAA_SUBSONIC_SEL         1
#define RIAA_ENABLE               2
#define RIAA_STORE_SETTINGS       3
#define RIAA_DECLICK_ENABLE       4
#define RIAA_SPIKE_THRESHOLD      5
#define RIAA_SPIKE_WIDTH          6
#define RIAA_CLIPPED_SAMPLES      7
#define RIAA_DETECTED_CLICKS      8
#define RIAA_INPUT_L              9
#define RIAA_INPUT_R              10
#define RIAA_OUTPUT_L             11
#define RIAA_OUTPUT_R             12

typedef struct {
    LADSPA_Data *gain;
    LADSPA_Data *subsonic_sel;
    LADSPA_Data *riaa_enable;
    LADSPA_Data *store_settings;
    LADSPA_Data *declick_enable;
    LADSPA_Data *spike_threshold;
    LADSPA_Data *spike_width;
    LADSPA_Data *clipped_samples;
    LADSPA_Data *detected_clicks;
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
    
    // Default values from config file
    LADSPA_Data default_gain;
    LADSPA_Data default_subsonic_sel;
    LADSPA_Data default_riaa_enable;
    
    // RIAA processing states for left and right channels
    RIAAChannelState channel_l;
    RIAAChannelState channel_r;
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
        case RIAA_CLIPPED_SAMPLES:
            plugin->clipped_samples = data;
            break;
        case RIAA_DETECTED_CLICKS:
            plugin->detected_clicks = data;
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
}

static void run_RIAA(
    LADSPA_Handle instance,
    unsigned long sample_count) {
    
    RIAA *plugin = (RIAA *)instance;
    LADSPA_Data *input_l = plugin->input_l;
    LADSPA_Data *input_r = plugin->input_r;
    LADSPA_Data *output_l = plugin->output_l;
    LADSPA_Data *output_r = plugin->output_r;
    LADSPA_Data gain_db = *(plugin->gain);
    LADSPA_Data gain = db_to_voltage(gain_db);
    int subsonic_sel = (int)(*(plugin->subsonic_sel) + 0.5f);  // Round to nearest int
    int riaa_enable = (int)(*(plugin->riaa_enable) + 0.5f);  // Round to nearest int
    int declick_enable = (int)(*(plugin->declick_enable) + 0.5f);  // Round to nearest int
    
    // Update declick configuration from control ports
    plugin->declick_config.threshold = (int)(*(plugin->spike_threshold) + 0.5f);
    plugin->declick_config.click_width_ms = *(plugin->spike_width);
    
    // Copy input to output buffers first
    memcpy(output_l, input_l, sample_count * sizeof(LADSPA_Data));
    memcpy(output_r, input_r, sample_count * sizeof(LADSPA_Data));
    
    // Apply declick if enabled (process before RIAA)
    if (declick_enable && sample_count >= 4096) {
        int clicks_l = declick_process(output_l, sample_count, &plugin->declick_config, plugin->sample_rate);
        int clicks_r = declick_process(output_r, sample_count, &plugin->declick_config, plugin->sample_rate);
        for (int c = 0; c < clicks_l + clicks_r; c++) {
            counter_increment(&plugin->click_counter);
        }
    }
    
    for (unsigned long i = 0; i < sample_count; i++) {
        LADSPA_Data y_l = output_l[i];
        LADSPA_Data y_r = output_r[i];
        
        // Apply RIAA processing
        y_l = riaa_process_sample(&plugin->channel_l, y_l, subsonic_sel, riaa_enable);
        y_r = riaa_process_sample(&plugin->channel_r, y_r, subsonic_sel, riaa_enable);
        
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
            g_riaa_descriptor->PortCount = 13;
            
            LADSPA_PortDescriptor *port_descriptors = 
                (LADSPA_PortDescriptor *)calloc(13, sizeof(LADSPA_PortDescriptor));
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
            port_descriptors[RIAA_INPUT_L] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
            port_descriptors[RIAA_INPUT_R] = 
                LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
            port_descriptors[RIAA_OUTPUT_L] = 
                LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
            port_descriptors[RIAA_OUTPUT_R] = 
                LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
            g_riaa_descriptor->PortDescriptors = port_descriptors;
            
            const char **port_names = (const char **)calloc(13, sizeof(char *));
            port_names[RIAA_GAIN] = strdup(RIAA_PORT_NAME_GAIN);
            port_names[RIAA_SUBSONIC_SEL] = strdup(RIAA_PORT_NAME_SUBSONIC_FILTER);
            port_names[RIAA_ENABLE] = strdup(RIAA_PORT_NAME_ENABLE);
            port_names[RIAA_STORE_SETTINGS] = strdup(PORT_NAME_STORE_SETTINGS);
            port_names[RIAA_DECLICK_ENABLE] = strdup("Declick Enable");
            port_names[RIAA_SPIKE_THRESHOLD] = strdup("Spike RMS Threshold");
            port_names[RIAA_SPIKE_WIDTH] = strdup("Spike Width (ms)");
            port_names[RIAA_CLIPPED_SAMPLES] = strdup(PORT_NAME_CLIPPED_SAMPLES);
            port_names[RIAA_DETECTED_CLICKS] = strdup("Detected Clicks");
            port_names[RIAA_INPUT_L] = strdup(PORT_NAME_INPUT_L);
            port_names[RIAA_INPUT_R] = strdup(PORT_NAME_INPUT_R);
            port_names[RIAA_OUTPUT_L] = strdup(PORT_NAME_OUTPUT_L);
            port_names[RIAA_OUTPUT_R] = strdup(PORT_NAME_OUTPUT_R);
            g_riaa_descriptor->PortNames = port_names;
            
            LADSPA_PortRangeHint *port_range_hints = 
                (LADSPA_PortRangeHint *)calloc(13, sizeof(LADSPA_PortRangeHint));
            
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
                LADSPA_HINT_BOUNDED_BELOW | 
                LADSPA_HINT_BOUNDED_ABOVE |
                LADSPA_HINT_INTEGER |
                LADSPA_HINT_TOGGLED |
                LADSPA_HINT_DEFAULT_1;
            port_range_hints[RIAA_ENABLE].LowerBound = 0.0f;
            port_range_hints[RIAA_ENABLE].UpperBound = 1.0f;
            
            // Store settings: 0 = no action, != 0 = save settings, default = 0 (no action)
            port_range_hints[RIAA_STORE_SETTINGS].HintDescriptor = 
                LADSPA_HINT_BOUNDED_BELOW | 
                LADSPA_HINT_BOUNDED_ABOVE |
                LADSPA_HINT_INTEGER |
                LADSPA_HINT_TOGGLED |
                LADSPA_HINT_DEFAULT_0;
            port_range_hints[RIAA_STORE_SETTINGS].LowerBound = 0.0f;
            port_range_hints[RIAA_STORE_SETTINGS].UpperBound = 1.0f;
            
            // Declick enable: 0 = off, 1 = on, default = 0 (disabled)
            port_range_hints[RIAA_DECLICK_ENABLE].HintDescriptor = 
                LADSPA_HINT_BOUNDED_BELOW | 
                LADSPA_HINT_BOUNDED_ABOVE |
                LADSPA_HINT_INTEGER |
                LADSPA_HINT_TOGGLED |
                LADSPA_HINT_DEFAULT_0;
            port_range_hints[RIAA_DECLICK_ENABLE].LowerBound = 0.0f;
            port_range_hints[RIAA_DECLICK_ENABLE].UpperBound = 1.0f;
            
            // Spike RMS Threshold: 1 to 900, default = 150
            port_range_hints[RIAA_SPIKE_THRESHOLD].HintDescriptor = 
                LADSPA_HINT_BOUNDED_BELOW | 
                LADSPA_HINT_BOUNDED_ABOVE |
                LADSPA_HINT_INTEGER |
                LADSPA_HINT_DEFAULT_MIDDLE;
            port_range_hints[RIAA_SPIKE_THRESHOLD].LowerBound = 1.0f;
            port_range_hints[RIAA_SPIKE_THRESHOLD].UpperBound = 900.0f;
            
            // Spike Width: 0.1 to 10.0 ms, default = 1.0 ms
            port_range_hints[RIAA_SPIKE_WIDTH].HintDescriptor = 
                LADSPA_HINT_BOUNDED_BELOW | 
                LADSPA_HINT_BOUNDED_ABOVE |
                LADSPA_HINT_DEFAULT_1;
            port_range_hints[RIAA_SPIKE_WIDTH].LowerBound = 0.1f;
            port_range_hints[RIAA_SPIKE_WIDTH].UpperBound = 10.0f;
            
            port_range_hints[RIAA_DETECTED_CLICKS].HintDescriptor = 0;
            port_range_hints[RIAA_CLIPPED_SAMPLES].HintDescriptor = 0;
            
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
