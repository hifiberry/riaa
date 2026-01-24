/*
 * biquad.c - Biquad filter coefficient generation
 * 
 * Functions to generate filter coefficients for various filter types
 */

#include <math.h>
#include "biquad.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * Generate Butterworth highpass filter coefficients
 * 
 * @param coeffs Output coefficient structure
 * @param sample_rate Sample rate in Hz
 * @param cutoff_freq Cutoff frequency in Hz
 * @param order Filter order (1 or 2)
 */
void biquad_highpass(BiquadCoeffs *coeffs, LADSPA_Data sample_rate, 
                     LADSPA_Data cutoff_freq, int order) {
    
    LADSPA_Data omega = 2.0 * M_PI * cutoff_freq / sample_rate;
    LADSPA_Data cos_omega = cos(omega);
    LADSPA_Data sin_omega = sin(omega);
    
    if (order == 1) {
        // 1st order highpass: H(z) = (1 - z^-1) / (1 + a*z^-1)
        // where a = (1 - tan(omega/2)) / (1 + tan(omega/2))
        LADSPA_Data tan_half = tan(omega / 2.0);
        LADSPA_Data a = (1.0 - tan_half) / (1.0 + tan_half);
        
        // Normalize for unity gain at Nyquist
        LADSPA_Data norm = (1.0 + a) / 2.0;
        
        coeffs->b0 = 1.0 / norm;
        coeffs->b1 = -1.0 / norm;
        coeffs->b2 = 0.0;
        coeffs->a1 = -a;  // Note: sign negated for our convention
        coeffs->a2 = 0.0;
        
    } else {
        // 2nd order Butterworth highpass
        // Q = 1/sqrt(2) = 0.7071 for Butterworth
        LADSPA_Data Q = 0.7071067811865476;
        LADSPA_Data alpha = sin_omega / (2.0 * Q);
        
        // Standard biquad form:
        // H(z) = [(1+cos)/2, -(1+cos), (1+cos)/2] / [1+alpha, -2*cos, 1-alpha]
        
        LADSPA_Data a0 = 1.0 + alpha;
        LADSPA_Data a1 = -2.0 * cos_omega;
        LADSPA_Data a2 = 1.0 - alpha;
        
        LADSPA_Data b0 = (1.0 + cos_omega) / 2.0;
        LADSPA_Data b1 = -(1.0 + cos_omega);
        LADSPA_Data b2 = (1.0 + cos_omega) / 2.0;
        
        // Normalize by a0
        coeffs->b0 = b0 / a0;
        coeffs->b1 = b1 / a0;
        coeffs->b2 = b2 / a0;
        coeffs->a1 = -a1 / a0;  // Note: sign negated for our convention
        coeffs->a2 = -a2 / a0;  // Note: sign negated for our convention
    }
}

/**
 * Generate Butterworth lowpass filter coefficients
 * 
 * @param coeffs Output coefficient structure
 * @param sample_rate Sample rate in Hz
 * @param cutoff_freq Cutoff frequency in Hz
 * @param order Filter order (1 or 2)
 */
void biquad_lowpass(BiquadCoeffs *coeffs, LADSPA_Data sample_rate,
                    LADSPA_Data cutoff_freq, int order) {
    
    LADSPA_Data omega = 2.0 * M_PI * cutoff_freq / sample_rate;
    LADSPA_Data cos_omega = cos(omega);
    LADSPA_Data sin_omega = sin(omega);
    
    if (order == 1) {
        // 1st order lowpass
        LADSPA_Data tan_half = tan(omega / 2.0);
        LADSPA_Data a = (tan_half - 1.0) / (tan_half + 1.0);
        
        LADSPA_Data norm = (1.0 - a) / 2.0;
        
        coeffs->b0 = 1.0 / norm;
        coeffs->b1 = 1.0 / norm;
        coeffs->b2 = 0.0;
        coeffs->a1 = -a;
        coeffs->a2 = 0.0;
        
    } else {
        // 2nd order Butterworth lowpass
        LADSPA_Data Q = 0.7071067811865476;
        LADSPA_Data alpha = sin_omega / (2.0 * Q);
        
        LADSPA_Data a0 = 1.0 + alpha;
        LADSPA_Data a1 = -2.0 * cos_omega;
        LADSPA_Data a2 = 1.0 - alpha;
        
        LADSPA_Data b0 = (1.0 - cos_omega) / 2.0;
        LADSPA_Data b1 = 1.0 - cos_omega;
        LADSPA_Data b2 = (1.0 - cos_omega) / 2.0;
        
        coeffs->b0 = b0 / a0;
        coeffs->b1 = b1 / a0;
        coeffs->b2 = b2 / a0;
        coeffs->a1 = -a1 / a0;
        coeffs->a2 = -a2 / a0;
    }
}
