/*
 * decibel.c - Decibel/multiplier conversion functions
 * 
 * Voltage-based decibel conversions (20 * log10)
 */

#include <math.h>
#include "decibel.h"

// Convert voltage ratio to decibels
// dB = 20 * log10(voltage_ratio)
double voltage_to_db(double voltage_ratio) {
    return 20.0 * log10(voltage_ratio);
}

// Convert decibels to voltage ratio
// voltage_ratio = 10^(dB/20)
double db_to_voltage(double db) {
    return pow(10.0, db / 20.0);
}
