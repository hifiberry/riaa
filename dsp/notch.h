/*
 * notch.h - Notch filter (band-stop) implementation
 */

#ifndef NOTCH_H
#define NOTCH_H

#include "biquad.h"

/*
 * Calculate notch filter coefficients
 * 
 * Parameters:
 *   coeffs: Pointer to biquad coefficients structure to fill
 *   freq: Notch center frequency in Hz
 *   q: Q factor (higher Q = narrower notch)
 *   sample_rate: Sample rate in Hz
 */
void calculate_notch_coeffs(BiquadCoeffs *coeffs, float freq, float q, float sample_rate);

#endif // NOTCH_H
