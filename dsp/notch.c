/*
 * notch.c - Notch filter (band-stop) implementation
 */

#include "notch.h"
#include <math.h>
#include <stdio.h>

void calculate_notch_coeffs(BiquadCoeffs *coeffs, float freq, float q, float sample_rate) {
    // Notch filter (band-stop) using biquad
    // Standard biquad notch filter:
    // H(z) = [1, -2*cos(w0), 1] / [1+alpha, -2*cos(w0), 1-alpha]
    // where w0 = 2*pi*f/fs and alpha = sin(w0)/(2*Q)
    
    float w0 = 2.0f * M_PI * freq / sample_rate;
    float cos_w0 = cosf(w0);
    float sin_w0 = sinf(w0);
    float alpha = sin_w0 / (2.0f * q);
    
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cos_w0;
    float a2 = 1.0f - alpha;
    
    float b0 = 1.0f;
    float b1 = -2.0f * cos_w0;
    float b2 = 1.0f;
    
    // Normalize by a0 and negate feedback coefficients for our convention
    coeffs->b0 = b0 / a0;
    coeffs->b1 = b1 / a0;
    coeffs->b2 = b2 / a0;
    coeffs->a1 = -a1 / a0;  // Note: sign negated for our convention
    coeffs->a2 = -a2 / a0;  // Note: sign negated for our convention
    
    fprintf(stderr, "Notch filter: freq=%.1f Q=%.1f fs=%.1f\n", freq, q, sample_rate);
    fprintf(stderr, "  w0=%.6f cos=%.6f sin=%.6f alpha=%.6f\n", w0, cos_w0, sin_w0, alpha);
    fprintf(stderr, "  b0=%.6f b1=%.6f b2=%.6f\n", coeffs->b0, coeffs->b1, coeffs->b2);
    fprintf(stderr, "  a1=%.6f a2=%.6f\n", coeffs->a1, coeffs->a2);
}
