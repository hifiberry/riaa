/*
 * riaa_calc.h - RIAA equalization and subsonic filter processing
 * 
 * Functions for applying RIAA equalization and subsonic filtering
 */

#ifndef RIAA_CALC_H
#define RIAA_CALC_H

#include <stddef.h>
#include "biquad.h"
#include "riaa_coeffs.h"

// Subsonic filter modes
#define SUBSONIC_OFF       0
#define SUBSONIC_1ST_ORDER 1
#define SUBSONIC_2ND_ORDER 2

/*
 * RIAA processing state for one channel
 */
typedef struct {
    BiquadState riaa_state;
    BiquadState subsonic_state;
    const BiquadCoeffs *riaa_coeffs;
    const BiquadCoeffs *subsonic_1st_coeffs;
    const BiquadCoeffs *subsonic_2nd_coeffs;
} RIAAChannelState;

/*
 * Initialize RIAA channel state for a given sample rate
 * 
 * Parameters:
 *   state: Channel state to initialize
 *   sample_rate_idx: Index into coefficient tables (0-5)
 */
void riaa_channel_init(RIAAChannelState *state, int sample_rate_idx);

/*
 * Reset RIAA channel state (clear filter memory)
 */
void riaa_channel_reset(RIAAChannelState *state);

/*
 * Process one sample through RIAA equalization and optional subsonic filter
 * 
 * Parameters:
 *   state: Channel state
 *   sample: Input sample
 *   subsonic_mode: SUBSONIC_OFF, SUBSONIC_1ST_ORDER, or SUBSONIC_2ND_ORDER
 *   riaa_enable: 1 to apply RIAA, 0 to bypass
 * 
 * Returns:
 *   Processed sample
 */
float riaa_process_sample(RIAAChannelState *state, float sample, 
                          int subsonic_mode, int riaa_enable);

#endif // RIAA_CALC_H
