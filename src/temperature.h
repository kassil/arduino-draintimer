#pragma once
#include <stdint.h>

// Initialize temperature sampling subsystem. Call once from setup().
void temperature_init();

// Call frequently from main loop to sample ADC and update smoothed values.
// The sampler accumulates SAMPLE_COUNT samples internally and updates the
// cached mean when ready.
void temperature_tick();

// Query whether a fresh mean is available (optional).
bool temperature_has_update();

// Get latest averaged ADC value (0..ADC_MAX). Returns false if not available.
bool temperature_get_adc_mean(uint16_t &out_adc);

// Get latest computed temperature in Celsius. If no valid reading returns NaN.
float temperature_get_celsius();

// Convert a raw ADC reading (0..1023) to Celsius using the thermistor model.
float temperature_adc_to_celsius(uint16_t adc);

// Access thresholds (ADC counts) used by heater/dispense logic
uint16_t temperature_get_adc_on_threshold();
uint16_t temperature_get_adc_off_threshold();
