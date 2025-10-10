#pragma once
#include <stdint.h>

void heating_init();
// Run the heater control
// @param the current heater state (LOW = ON, HIGH = OFF)
// @return new heater state (LOW = ON, HIGH = OFF)
uint8_t heating_loop(uint8_t heaterState);

void dispense_init();
uint8_t dispense_loop(uint8_t relayState);

// Convert a single ADC reading (0..1023) from the thermistor input into Celsius
float adc_to_celsius(uint16_t adc);
