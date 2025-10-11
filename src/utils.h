#pragma once
#include <stdint.h>

void lcd_print_left_justify(uint16_t value, uint8_t width);
void lcd_print_right_justify(uint16_t value, uint8_t width);
void lcd_print_temperature(float temp_c);

void pilot_init();
void pilot_off();
void pilot_on();

// Check the IO expander for errors. If an error is detected this will
// disable the pilot and clear the relays for safety then enter the fault
// menu. This is a best-effort safety hook and should be called frequently
// (for example once per main loop iteration).
void check_relay_error_and_fault();
