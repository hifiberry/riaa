/*
 * riaa_calc.c - RIAA equalization and subsonic filter processing
 */

#include "riaa_calc.h"
#include <string.h>

void riaa_channel_init(RIAAChannelState *state, int sample_rate_idx)
{
    // Set coefficient pointers for this sample rate
    state->riaa_coeffs = &RIAA_COEFFS[sample_rate_idx];
    state->subsonic_1st_coeffs = &SUBSONIC_1ST_ORDER_COEFFS[sample_rate_idx];
    state->subsonic_2nd_coeffs = &SUBSONIC_2ND_ORDER_COEFFS[sample_rate_idx];
    
    // Clear filter states
    memset(&state->riaa_state, 0, sizeof(BiquadState));
    memset(&state->subsonic_state, 0, sizeof(BiquadState));
}

void riaa_channel_reset(RIAAChannelState *state)
{
    memset(&state->riaa_state, 0, sizeof(BiquadState));
    memset(&state->subsonic_state, 0, sizeof(BiquadState));
}

float riaa_process_sample(RIAAChannelState *state, float sample, 
                          int subsonic_mode, int riaa_enable)
{
    float y = sample;
    
    // Apply subsonic filter first if enabled (20Hz highpass)
    if (subsonic_mode == SUBSONIC_1ST_ORDER) {
        y = process_biquad(state->subsonic_1st_coeffs, &state->subsonic_state, y);
    } else if (subsonic_mode == SUBSONIC_2ND_ORDER) {
        y = process_biquad(state->subsonic_2nd_coeffs, &state->subsonic_state, y);
    }
    
    // Apply RIAA equalization if enabled
    if (riaa_enable) {
        y = process_biquad(state->riaa_coeffs, &state->riaa_state, y);
    }
    
    return y;
}
