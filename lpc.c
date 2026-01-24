/*
 * lpc.c - Linear Predictive Coding implementation
 * 
 * Implements LPC analysis using autocorrelation method and Levinson-Durbin
 * recursion for coefficient calculation.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "lpc.h"

int lpc_init(LPCPredictor *predictor, int order) {
    if (!predictor || order < 1 || order > LPC_MAX_ORDER) {
        return -1;
    }
    
    predictor->order = order;
    memset(predictor->coeffs, 0, sizeof(predictor->coeffs));
    memset(predictor->history, 0, sizeof(predictor->history));
    predictor->history_pos = 0;
    
    return 0;
}

void lpc_reset(LPCPredictor *predictor) {
    if (!predictor) return;
    
    memset(predictor->history, 0, sizeof(predictor->history));
    predictor->history_pos = 0;
}

/**
 * Calculate autocorrelation coefficients
 * r[k] = sum(x[n] * x[n-k]) for k = 0..order
 */
static void calculate_autocorrelation(const LADSPA_Data *signal, int length, 
                                     LADSPA_Data *r, int order) {
    for (int k = 0; k <= order; k++) {
        r[k] = 0.0f;
        for (int n = k; n < length; n++) {
            r[k] += signal[n] * signal[n - k];
        }
    }
}

/**
 * Levinson-Durbin recursion to solve for LPC coefficients
 * Converts autocorrelation to LPC coefficients
 */
static int levinson_durbin(const LADSPA_Data *r, LADSPA_Data *coeffs, int order) {
    LADSPA_Data a[LPC_MAX_ORDER];      // Temporary coefficients
    LADSPA_Data k[LPC_MAX_ORDER];      // Reflection coefficients
    LADSPA_Data e;                      // Prediction error
    
    // Check for zero signal energy
    if (r[0] <= 0.0f) {
        return -1;
    }
    
    // Initialize
    e = r[0];
    
    // Levinson-Durbin recursion
    for (int i = 0; i < order; i++) {
        // Calculate reflection coefficient
        LADSPA_Data sum = r[i + 1];
        for (int j = 0; j < i; j++) {
            sum -= a[j] * r[i - j];
        }
        k[i] = sum / e;
        
        // Update coefficients using temporary storage
        LADSPA_Data temp[LPC_MAX_ORDER];
        for (int j = 0; j < i; j++) {
            temp[j] = a[j] - k[i] * a[i - j - 1];
        }
        for (int j = 0; j < i; j++) {
            a[j] = temp[j];
        }
        a[i] = k[i];
        
        // Update prediction error
        e *= (1.0f - k[i] * k[i]);
        
        // Check for numerical stability
        if (e <= 0.0f) {
            return -1;
        }
    }
    
    // Copy final coefficients (negate them for prediction formula)
    for (int i = 0; i < order; i++) {
        coeffs[i] = -a[i];
    }
    
    return 0;
}

int lpc_analyze(LPCPredictor *predictor, const LADSPA_Data *signal, int length) {
    if (!predictor || !signal || length < predictor->order + 1) {
        return -1;
    }
    
    // Calculate autocorrelation coefficients
    LADSPA_Data r[LPC_MAX_ORDER + 1];
    calculate_autocorrelation(signal, length, r, predictor->order);
    
    // Solve for LPC coefficients using Levinson-Durbin
    return levinson_durbin(r, predictor->coeffs, predictor->order);
}

LADSPA_Data lpc_predict(const LPCPredictor *predictor) {
    if (!predictor) return 0.0f;
    
    LADSPA_Data prediction = 0.0f;
    int pos = predictor->history_pos;
    
    // Predict based on weighted sum of past samples
    // prediction = sum(a[i] * x[n-i-1]) for i = 0..order-1
    // Coefficients are already negated, so we add them directly
    for (int i = 0; i < predictor->order; i++) {
        // Go backwards through history (most recent first)
        pos = (pos - 1 + LPC_MAX_ORDER) % LPC_MAX_ORDER;
        prediction -= predictor->coeffs[i] * predictor->history[pos];
    }
    
    return prediction;
}

void lpc_update(LPCPredictor *predictor, LADSPA_Data sample) {
    if (!predictor) return;
    
    predictor->history[predictor->history_pos] = sample;
    predictor->history_pos = (predictor->history_pos + 1) % LPC_MAX_ORDER;
}

LADSPA_Data lpc_predict_error(LPCPredictor *predictor, 
                               LADSPA_Data actual_sample,
                               LADSPA_Data *prediction) {
    if (!predictor) return 0.0f;
    
    // Get prediction
    LADSPA_Data predicted = lpc_predict(predictor);
    
    // Store prediction if requested
    if (prediction) {
        *prediction = predicted;
    }
    
    // Update history with actual sample
    lpc_update(predictor, actual_sample);
    
    // Return error
    return actual_sample - predicted;
}
