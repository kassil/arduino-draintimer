#include "utils.h"
#include "main.h"
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

// Pilot GPIO: dedicated ATmega output, active low
// The pilot relay coil is driven by a transistor on the MCU pin.  The load
// side grounds the control coils of the other relays.
constexpr uint8_t PILOT_PIN = 7;
// Milliseconds to wait after enabling the pilot so the relay can energize.
constexpr uint8_t PILOT_SETTLE_MS = 20;

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

void pilot_init()
{
    pinMode(PILOT_PIN, OUTPUT);
    pilot_off();
}

void pilot_on()
{
    digitalWrite(PILOT_PIN, LOW); // active low
    delay(PILOT_SETTLE_MS);
}

void pilot_off()
{
    digitalWrite(PILOT_PIN, HIGH); // Pilot OFF (inactive)
}
