/*
 * decibel.h - Decibel/multiplier conversion functions
 * 
 * Voltage-based decibel conversions (20 * log10)
 */

#ifndef DECIBEL_H
#define DECIBEL_H

// Convert voltage ratio to decibels
// dB = 20 * log10(voltage_ratio)
double voltage_to_db(double voltage_ratio);

// Convert decibels to voltage ratio
// voltage_ratio = 10^(dB/20)
double db_to_voltage(double db);

#endif // DECIBEL_H
