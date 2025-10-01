#include "utils.h"
#include "drain_timer.h"
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

// Print a non-negative integer left-justified in a fixed-width field
void lcd_print_left_justify(uint16_t value, uint8_t width) {
    // Print the number itself
    lcd.print(value);

    // Count how many digits we printed
    uint16_t tmp = value;
    uint8_t digits = 1;
    while (tmp >= 10) {
        tmp /= 10;
        digits++;
    }

    // Pad with spaces to clear the rest of the field
    for (uint8_t i = digits; i < width; i++) {
        lcd.print(' ');
    }
}

// Print a non-negative integer right-justified in a fixed-width field
void lcd_print_right_justify(uint16_t value, uint8_t width) {
    // Count digits in the value
    uint16_t tmp = value;
    uint8_t digits = 1;
    while (tmp >= 10) {
        tmp /= 10;
        digits++;
    }

    // Print leading spaces if needed
    for (uint8_t i = digits; i < width; i++) {
        lcd.print(' ');
    }

    // Print the number itself
    lcd.print(value);
}
