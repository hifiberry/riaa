/*
 * test_lpc.c - Test LPC predictor functionality
 * 
 * Tests LPC analysis and prediction on synthetic signals
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "lpc.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE 48000

int main(void) {
    printf("LPC Predictor Test\n");
    printf("==================\n\n");
    
    // Test 1: Simple sine wave prediction
    printf("Test 1: Sine wave prediction\n");
    printf("-----------------------------\n");
    
    LPCPredictor predictor;
    int order = 8;
    
    if (lpc_init(&predictor, order) != 0) {
        fprintf(stderr, "Failed to initialize LPC predictor\n");
        return 1;
    }
    
    printf("Predictor order: %d\n", order);
    
    // Generate a sine wave segment for analysis
    int analysis_length = 256;
    LADSPA_Data *signal = (LADSPA_Data*)malloc(analysis_length * sizeof(LADSPA_Data));
    
    float frequency = 1000.0f;
    for (int i = 0; i < analysis_length; i++) {
        signal[i] = sinf(2.0f * M_PI * frequency * i / SAMPLE_RATE);
    }
    
    // Analyze signal to get LPC coefficients
    printf("Analyzing signal...\n");
    if (lpc_analyze(&predictor, signal, analysis_length) != 0) {
        fprintf(stderr, "LPC analysis failed\n");
        free(signal);
        return 1;
    }
    
    printf("LPC coefficients calculated\n");
    printf("First 4 coefficients: %.6f, %.6f, %.6f, %.6f\n",
           predictor.coeffs[0], predictor.coeffs[1], 
           predictor.coeffs[2], predictor.coeffs[3]);
    
    // Prime the predictor with initial samples
    lpc_reset(&predictor);
    for (int i = 0; i < order; i++) {
        lpc_update(&predictor, signal[i]);
    }
    
    // Test prediction on next samples
    printf("\nPrediction test (next 10 samples):\n");
    printf("Sample   Actual      Predicted   Error      Abs Error\n");
    printf("------   --------    --------    --------   ---------\n");
    
    double total_error = 0.0;
    double total_abs_error = 0.0;
    int test_count = 10;
    
    for (int i = order; i < order + test_count; i++) {
        LADSPA_Data predicted;
        LADSPA_Data error = lpc_predict_error(&predictor, signal[i], &predicted);
        double abs_error = fabs(error);
        
        printf("%-6d   %8.6f    %8.6f    %8.6f   %9.6f\n",
               i, signal[i], predicted, error, abs_error);
        
        total_error += error;
        total_abs_error += abs_error;
    }
    
    printf("\nMean error: %.6f\n", total_error / test_count);
    printf("Mean absolute error: %.6f\n\n", total_abs_error / test_count);
    
    // Test 2: Click detection using prediction error
    printf("Test 2: Click detection via prediction error\n");
    printf("---------------------------------------------\n");
    
    // Reset and re-analyze
    lpc_reset(&predictor);
    
    // Generate signal with a click
    int signal_length = 512;
    free(signal);
    signal = (LADSPA_Data*)malloc(signal_length * sizeof(LADSPA_Data));
    
    for (int i = 0; i < signal_length; i++) {
        signal[i] = 0.5f * sinf(2.0f * M_PI * frequency * i / SAMPLE_RATE);
    }
    
    // Add a click at position 256
    int click_pos = 256;
    signal[click_pos] += 2.0f;
    
    printf("Signal: sine wave with click at sample %d\n", click_pos);
    
    // Analyze first part (before click)
    lpc_analyze(&predictor, signal, click_pos - order);
    
    // Prime predictor
    lpc_reset(&predictor);
    for (int i = click_pos - order; i < click_pos; i++) {
        lpc_update(&predictor, signal[i]);
    }
    
    // Test prediction around the click
    printf("\nPrediction errors around click:\n");
    printf("Sample   Actual      Predicted   Error      Abs Error\n");
    printf("------   --------    --------    --------   ---------\n");
    
    for (int i = click_pos - 5; i < click_pos + 5; i++) {
        if (i >= 0 && i < signal_length) {
            LADSPA_Data predicted;
            LADSPA_Data error = lpc_predict_error(&predictor, signal[i], &predicted);
            double abs_error = fabs(error);
            
            char marker = (i == click_pos) ? '*' : ' ';
            printf("%-6d %c %8.6f    %8.6f    %8.6f   %9.6f\n",
                   i, marker, signal[i], predicted, error, abs_error);
        }
    }
    
    printf("\n* = Click position (note large prediction error)\n\n");
    
    // Test 3: Different predictor orders
    printf("Test 3: Effect of predictor order\n");
    printf("----------------------------------\n");
    
    int orders[] = {2, 4, 8, 16};
    int num_orders = sizeof(orders) / sizeof(orders[0]);
    
    // Clean signal for comparison
    for (int i = 0; i < analysis_length; i++) {
        signal[i] = sinf(2.0f * M_PI * frequency * i / SAMPLE_RATE);
    }
    
    printf("Order    Mean Abs Error\n");
    printf("-----    --------------\n");
    
    for (int o = 0; o < num_orders; o++) {
        LPCPredictor test_pred;
        lpc_init(&test_pred, orders[o]);
        lpc_analyze(&test_pred, signal, analysis_length);
        
        // Prime and test
        lpc_reset(&test_pred);
        for (int i = 0; i < orders[o]; i++) {
            lpc_update(&test_pred, signal[i]);
        }
        
        double sum_abs_error = 0.0;
        int count = 50;
        for (int i = orders[o]; i < orders[o] + count; i++) {
            LADSPA_Data predicted;
            LADSPA_Data error = lpc_predict_error(&test_pred, signal[i], &predicted);
            sum_abs_error += fabs(error);
        }
        
        printf("%-5d    %.8f\n", orders[o], sum_abs_error / count);
    }
    
    printf("\nLPC tests completed successfully!\n");
    
    free(signal);
    return 0;
}
