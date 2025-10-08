#pragma once
#include <stdint.h>

void heating_init();
void heating_loop();

void dispense_init();
void dispense_loop();

// Convert a single ADC reading (0..1023) from the thermistor input into Celsius
float adc_to_celsius(uint16_t adc);
