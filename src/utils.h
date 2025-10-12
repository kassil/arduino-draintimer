#pragma once
#include <stdint.h>

void lcd_print_left_justify(uint16_t value, uint8_t width);
void lcd_print_right_justify(uint16_t value, uint8_t width, char padding);
void lcd_print_temperature(float temp_c);

// Print a two-digit non-negative number (0..99) with left padding.
// If value >= 100, prints "??". 'pad' is the padding character used on the left
// to extend the field to the requested width (width must be >= 2).
void lcd_print_right_justify_2d(uint8_t value, char pad);

// Check the IO expander for errors. If an error is detected this will
// disable the pilot and clear the relays for safety then enter the fault
// menu. This is a best-effort safety hook and should be called frequently
// (for example once per main loop iteration).
void check_relay_error_and_fault();
