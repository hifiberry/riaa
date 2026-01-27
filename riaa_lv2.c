/*
 * riaa_lv2.c - RIAA Equalization LV2 Plugin
 * 
 * LV2 wrapper for RIAA phono preamp equalization with subsonic filtering,
 * declick, and notch filter capabilities.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "lv2/lv2plug.in/ns/lv2core/lv2.h"
#include "dsp/biquad.h"
#include "dsp/riaa_coeffs.h"
#include "dsp/riaa_calc.h"
#include "dsp/decibel.h"
#include "utils/counter.h"
#include "dsp/declick.h"
#include "dsp/samplerate.h"
#include "dsp/notch.h"

#define RIAA_URI "http://hifiberry.com/lv2/riaa"

typedef enum {
    RIAA_GAIN = 0,
    RIAA_SUBSONIC_SEL = 1,
    RIAA_ENABLE = 2,
    RIAA_DECLICK_ENABLE = 3,
    RIAA_SPIKE_THRESHOLD = 4,
    RIAA_SPIKE_WIDTH = 5,
    RIAA_NOTCH_ENABLE = 6,
    RIAA_NOTCH_FREQ = 7,
    RIAA_NOTCH_Q = 8,
    RIAA_CLIPPED_SAMPLES = 9,
    RIAA_DETECTED_CLICKS = 10,
    RIAA_AVG_SPIKE_LENGTH = 11,
    RIAA_AVG_RMS_DB = 12,
    RIAA_INPUT_L = 13,
    RIAA_INPUT_R = 14,
    RIAA_OUTPUT_L = 15,
    RIAA_OUTPUT_R = 16
} PortIndex;

typedef struct {
    // Port buffers
    const float* gain;
    const float* subsonic_sel;
    const float* riaa_enable;
    const float* declick_enable;
    const float* spike_threshold;
    const float* spike_width;
    const float* notch_enable;
    const float* notch_freq;
    const float* notch_q;
    float* clipped_samples;
    float* detected_clicks;
    float* avg_spike_length;
    float* avg_rms_db;
    const float* input_l;
    const float* input_r;
    float* output_l;
    float* output_r;
    
    Counter clip_counter;
    Counter click_counter;
    int sample_rate_idx;
    uint32_t sample_rate;
    
    // Declick configuration
    DeclickConfig declick_config;
    
    // Accumulated declick statistics
    double total_spike_length_sum;
    double total_log_rms_sum;
    int total_rms_samples;
    
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

static LV2_Handle
instantiate(const LV2_Descriptor* descriptor,
            double rate,
            const char* bundle_path,
            const LV2_Feature* const* features)
{
    RIAA* plugin = (RIAA*)calloc(1, sizeof(RIAA));
    if (!plugin) {
        return NULL;
    }
    
    plugin->sample_rate = (uint32_t)rate;
    
    counter_init(&plugin->clip_counter);
    counter_init(&plugin->click_counter);
    
    // Initialize declick configuration
    declick_config_init(&plugin->declick_config);
    plugin->declick_config.threshold = 150;
    plugin->declick_config.click_width_ms = 1.0f;
    
    // Initialize accumulated statistics
    plugin->total_spike_length_sum = 0.0;
    plugin->total_log_rms_sum = 0.0;
    plugin->total_rms_samples = 0;
    
    // Initialize notch filter state
    memset(&plugin->notch_state_l, 0, sizeof(BiquadState));
    memset(&plugin->notch_state_r, 0, sizeof(BiquadState));
    plugin->last_notch_freq = 0.0f;
    plugin->last_notch_q = 0.0f;
    
    // Find the sample rate index
    plugin->sample_rate_idx = get_sample_rate_index(plugin->sample_rate);
    
    if (plugin->sample_rate_idx < 0) {
        fprintf(stderr, "RIAA LV2: Unsupported sample rate %u Hz\n", plugin->sample_rate);
        fprintf(stderr, "RIAA LV2: Supported rates: 44.1, 48, 88.2, 96, 176.4, 192 kHz\n");
        free(plugin);
        return NULL;
    }
    
    // Initialize RIAA processing for both channels
    riaa_channel_init(&plugin->channel_l, plugin->sample_rate_idx);
    riaa_channel_init(&plugin->channel_r, plugin->sample_rate_idx);
    
    fprintf(stderr, "RIAA LV2: Initialized at %u Hz (index %d)\n", 
            plugin->sample_rate, plugin->sample_rate_idx);
    
    return (LV2_Handle)plugin;
}

static void
connect_port(LV2_Handle instance,
             uint32_t port,
             void* data)
{
    RIAA* plugin = (RIAA*)instance;
    
    switch ((PortIndex)port) {
        case RIAA_GAIN:
            plugin->gain = (const float*)data;
            break;
        case RIAA_SUBSONIC_SEL:
            plugin->subsonic_sel = (const float*)data;
            break;
        case RIAA_ENABLE:
            plugin->riaa_enable = (const float*)data;
            break;
        case RIAA_DECLICK_ENABLE:
            plugin->declick_enable = (const float*)data;
            break;
        case RIAA_SPIKE_THRESHOLD:
            plugin->spike_threshold = (const float*)data;
            break;
        case RIAA_SPIKE_WIDTH:
            plugin->spike_width = (const float*)data;
            break;
        case RIAA_NOTCH_ENABLE:
            plugin->notch_enable = (const float*)data;
            break;
        case RIAA_NOTCH_FREQ:
            plugin->notch_freq = (const float*)data;
            break;
        case RIAA_NOTCH_Q:
            plugin->notch_q = (const float*)data;
            break;
        case RIAA_CLIPPED_SAMPLES:
            plugin->clipped_samples = (float*)data;
            break;
        case RIAA_DETECTED_CLICKS:
            plugin->detected_clicks = (float*)data;
            break;
        case RIAA_AVG_SPIKE_LENGTH:
            plugin->avg_spike_length = (float*)data;
            break;
        case RIAA_AVG_RMS_DB:
            plugin->avg_rms_db = (float*)data;
            break;
        case RIAA_INPUT_L:
            plugin->input_l = (const float*)data;
            break;
        case RIAA_INPUT_R:
            plugin->input_r = (const float*)data;
            break;
        case RIAA_OUTPUT_L:
            plugin->output_l = (float*)data;
            break;
        case RIAA_OUTPUT_R:
            plugin->output_r = (float*)data;
            break;
    }
}

static void
activate(LV2_Handle instance)
{
    RIAA* plugin = (RIAA*)instance;
    
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
    
    // Debug: Show initial state
    int riaa_enable = plugin->riaa_enable ? (int)(*plugin->riaa_enable + 0.5f) : 1;
    int declick_enable = plugin->declick_enable ? (int)(*plugin->declick_enable + 0.5f) : 0;
    int notch_enable = plugin->notch_enable ? (int)(*plugin->notch_enable + 0.5f) : 0;
    fprintf(stderr, "RIAA LV2: Activated - RIAA:%s Declick:%s Notch:%s\n", 
            riaa_enable ? "ON" : "OFF",
            declick_enable ? "ON" : "OFF",
            notch_enable ? "ON" : "OFF");
}

static void
run(LV2_Handle instance, uint32_t sample_count)
{
    RIAA* plugin = (RIAA*)instance;
    
    const float* input_l = plugin->input_l;
    const float* input_r = plugin->input_r;
    float* output_l = plugin->output_l;
    float* output_r = plugin->output_r;
    
    // Read control ports
    float gain_db = plugin->gain ? *plugin->gain : 0.0f;
    float gain = db_to_voltage(gain_db);
    int subsonic_sel = plugin->subsonic_sel ? (int)(*plugin->subsonic_sel + 0.5f) : 0;
    int riaa_enable = plugin->riaa_enable ? (int)(*plugin->riaa_enable + 0.5f) : 1;
    int declick_enable = plugin->declick_enable ? (int)(*plugin->declick_enable + 0.5f) : 0;
    int notch_enable = plugin->notch_enable ? (int)(*plugin->notch_enable + 0.5f) : 0;
    float notch_freq = plugin->notch_freq ? *plugin->notch_freq : 50.0f;
    float notch_q = plugin->notch_q ? *plugin->notch_q : 10.0f;
    
    // Update notch filter coefficients if parameters changed
    if (notch_enable && (notch_freq != plugin->last_notch_freq || notch_q != plugin->last_notch_q)) {
        calculate_notch_coeffs(&plugin->notch_coeffs, notch_freq, notch_q, plugin->sample_rate);
        plugin->last_notch_freq = notch_freq;
        plugin->last_notch_q = notch_q;
    }
    
    // Update declick configuration from control ports
    if (plugin->spike_threshold) {
        float threshold_db = *plugin->spike_threshold;
        float voltage_ratio = powf(10.0f, threshold_db / 20.0f);
        plugin->declick_config.threshold = (int)(voltage_ratio * 9.0f + 0.5f);
        if (plugin->declick_config.threshold < 1) plugin->declick_config.threshold = 1;
        if (plugin->declick_config.threshold > 900) plugin->declick_config.threshold = 900;
    }
    if (plugin->spike_width) {
        plugin->declick_config.click_width_ms = *plugin->spike_width;
    }
    
    // Copy input to output buffers first
    memcpy(output_l, input_l, sample_count * sizeof(float));
    memcpy(output_r, input_r, sample_count * sizeof(float));
    
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
    
    for (uint32_t i = 0; i < sample_count; i++) {
        float y_l = output_l[i];
        float y_r = output_r[i];
        
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
        *plugin->clipped_samples = (float)counter_get(&plugin->clip_counter);
    }
    if (plugin->detected_clicks) {
        *plugin->detected_clicks = (float)counter_get(&plugin->click_counter);
    }
    
    // Update statistics output ports
    unsigned long total_clicks = counter_get(&plugin->click_counter);
    if (plugin->avg_spike_length) {
        if (total_clicks > 0) {
            *plugin->avg_spike_length = (float)(plugin->total_spike_length_sum / total_clicks);
        } else {
            *plugin->avg_spike_length = 0.0f;
        }
    }
    if (plugin->avg_rms_db) {
        if (plugin->total_rms_samples > 0) {
            double avg_ratio_linear = plugin->total_log_rms_sum / plugin->total_rms_samples;
            *plugin->avg_rms_db = (float)(10.0 * log10(avg_ratio_linear));
        } else {
            *plugin->avg_rms_db = 0.0f;
        }
    }
}

static void
deactivate(LV2_Handle instance)
{
    // Nothing to do here
}

static void
cleanup(LV2_Handle instance)
{
    free(instance);
}

static const void*
extension_data(const char* uri)
{
    return NULL;
}

static const LV2_Descriptor descriptor = {
    RIAA_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
    return index == 0 ? &descriptor : NULL;
}
