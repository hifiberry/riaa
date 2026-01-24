/*
 * lpc.h - Linear Predictive Coding (LPC) for audio processing
 * 
 * Implements LPC analysis and prediction for audio signal processing.
 * Useful for click detection and interpolation in audio restoration.
 */

#ifndef LPC_H
#define LPC_H

#include <ladspa.h>

// Maximum LPC order supported
#define LPC_MAX_ORDER 32

// LPC predictor structure
typedef struct {
    int order;                      // Predictor order (number of coefficients)
    LADSPA_Data coeffs[LPC_MAX_ORDER];  // LPC coefficients
    LADSPA_Data history[LPC_MAX_ORDER]; // Sample history buffer
    int history_pos;                // Current position in history buffer
} LPCPredictor;

/**
 * Initialize an LPC predictor
 * 
 * @param predictor Predictor structure to initialize
 * @param order Predictor order (1 to LPC_MAX_ORDER)
 * @return 0 on success, -1 on error
 */
int lpc_init(LPCPredictor *predictor, int order);

/**
 * Reset predictor history (clear past samples)
 * 
 * @param predictor Predictor to reset
 */
void lpc_reset(LPCPredictor *predictor);

/**
 * Calculate LPC coefficients from a signal segment using autocorrelation method
 * with Levinson-Durbin recursion
 * 
 * @param predictor Predictor to update with new coefficients
 * @param signal Input signal buffer
 * @param length Length of signal buffer
 * @return 0 on success, -1 on error
 */
int lpc_analyze(LPCPredictor *predictor, const LADSPA_Data *signal, int length);

/**
 * Predict the next sample value based on history
 * 
 * @param predictor Predictor with coefficients and history
 * @return Predicted sample value
 */
LADSPA_Data lpc_predict(const LPCPredictor *predictor);

/**
 * Update predictor history with a new sample
 * (Call this after getting the actual sample to maintain history)
 * 
 * @param predictor Predictor to update
 * @param sample New sample value
 */
void lpc_update(LPCPredictor *predictor, LADSPA_Data sample);

/**
 * Predict next sample and calculate prediction error
 * 
 * @param predictor Predictor with coefficients and history
 * @param actual_sample The actual sample value
 * @param prediction Output: predicted value (can be NULL)
 * @return Prediction error (actual - predicted)
 */
LADSPA_Data lpc_predict_error(LPCPredictor *predictor, 
                               LADSPA_Data actual_sample,
                               LADSPA_Data *prediction);

#endif // LPC_H
