#pragma once
#include <stdint.h>

// Run the heater control
// @param the current heater state (LOW = ON, HIGH = OFF)
// @return new heater state (LOW = ON, HIGH = OFF)
uint8_t heating_loop(uint8_t heaterState);

// Set up the dispenser control subsystem.
// Dispenser control does not actually send hardware commands.
void dispense_init();

// Run the dispenser control subsytem.
// @return LOW for on, HIGH for off
uint8_t dispense_loop();

// Query the last averaged ADC value from the thermistor (0..1023)
bool calc_analog_mean(uint8_t idx, uint16_t& adc_mean);
