#pragma once
#include <stdint.h>

// Set up the heater control loop.
// Heater control does not actually send hardware commands.
void heating_init();

// Run the heater control
// @param the current heater state (LOW = ON, HIGH = OFF)
// @return new heater state (LOW = ON, HIGH = OFF)
uint8_t heating_loop(uint8_t heaterState);

// Set up the dispenser control subsystem.
// Dispenser control does not actually send hardware commands.
void dispense_init();

// Run the dispenser control subsytem.
// @return LOW for on, HIGH for off
uint8_t dispense_loop(uint16_t temperature);

// Convert a single ADC reading (0..1023) from the thermistor input into Celsius
float adc_to_celsius(uint16_t adc);

bool calc_analog_mean(uint8_t idx, uint16_t& adc_mean);
