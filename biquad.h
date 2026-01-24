/*
 * biquad.h - Generic biquad filter structures and utilities
 * 
 * Biquad Filter Implementation
 * ============================
 * 
 * Standard Direct Form II difference equation:
 *   y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] + a1*y[n-1] + a2*y[n-2]
 * 
 * Coefficient Sign Convention:
 * ---------------------------
 * The coefficients are stored with NEGATED SIGNS compared to the typical
 * textbook transfer function notation.
 * 
 * Textbook form:   H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 - a1*z^-1 - a2*z^-2)
 * Storage form:    Store a1 and a2 with POSITIVE values for subtracted terms
 * 
 * Example for a typical highpass or lowpass filter:
 *   - If the transfer function denominator is (1 - 1.5*z^-1 + 0.6*z^-2)
 *   - Store: a1 = +1.5, a2 = -0.6
 *   - The implementation will compute: + a1*y[n-1] + a2*y[n-2]
 * 
 * Feedforward coefficients (b0, b1, b2):
 *   - Store as-is from the transfer function numerator
 *   - Can be positive or negative depending on filter design
 * 
 * Feedback coefficients (a1, a2):
 *   - a1: Typically POSITIVE (represents negated -a1 from denominator)
 *   - a2: Can be positive or negative depending on filter
 *   - For stability, poles must be inside unit circle
 */

#ifndef BIQUAD_H
#define BIQUAD_H

#include <ladspa.h>

// Biquad filter coefficient structure
typedef struct {
    LADSPA_Data b0, b1, b2;  // Feedforward coefficients (numerator)
    LADSPA_Data a1, a2;       // Feedback coefficients (denominator, a0=1.0, signs already negated)
} BiquadCoeffs;

// Biquad filter state structure
typedef struct {
    LADSPA_Data x1, x2;  // Input history (previous samples)
    LADSPA_Data y1, y2;  // Output history (previous samples)
} BiquadState;

// Helper function to process a single sample through a biquad filter
// Implements: y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] + a1*y[n-1] + a2*y[n-2]
// Note: a1 and a2 should be stored with signs already negated from textbook form
static inline LADSPA_Data process_biquad(
    const BiquadCoeffs *coeffs,
    BiquadState *state,
    LADSPA_Data input) {
    
    LADSPA_Data output = coeffs->b0 * input +
                         coeffs->b1 * state->x1 +
                         coeffs->b2 * state->x2 +
                         coeffs->a1 * state->y1 +
                         coeffs->a2 * state->y2;
    
    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;
    
    return output;
}

/**
 * Generate Butterworth highpass filter coefficients
 * 
 * @param coeffs Output coefficient structure
 * @param sample_rate Sample rate in Hz
 * @param cutoff_freq Cutoff frequency in Hz
 * @param order Filter order (1 or 2)
 */
void biquad_highpass(BiquadCoeffs *coeffs, LADSPA_Data sample_rate, 
                     LADSPA_Data cutoff_freq, int order);

/**
 * Generate Butterworth lowpass filter coefficients
 * 
 * @param coeffs Output coefficient structure
 * @param sample_rate Sample rate in Hz
 * @param cutoff_freq Cutoff frequency in Hz
 * @param order Filter order (1 or 2)
 */
void biquad_lowpass(BiquadCoeffs *coeffs, LADSPA_Data sample_rate,
                    LADSPA_Data cutoff_freq, int order);

#endif // BIQUAD_H
