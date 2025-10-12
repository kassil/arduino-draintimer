#include "utils.h"
#include "main.h"
#include "faults.h"
#include <PCF8574.h>
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
void lcd_print_right_justify(uint16_t value, uint8_t width, char padding) {
    // Count digits in the value
    uint16_t tmp = value;
    uint8_t digits = 1;
    while (tmp >= 10) {
        tmp /= 10;
        digits++;
    }

    // Print leading spaces if needed
    for (uint8_t i = digits; i < width; i++) {
        lcd.write(padding);
    }

    // Print the number itself
    lcd.print(value);
}

// Print a two-digit number 0..99 with configurable left padding.
// width is the total printed width (must be >= 2). pad is the fill character.
void lcd_print_right_justify_2d(uint8_t value, char pad)
{
    if (value >= 100) {
        // overflow
        lcd.print(F("++"));
        return;
    }
    if (value < 10) {
        lcd.print(pad);
    }
    else {
        // print tens (may be zero)
        uint8_t const tens = value / 10;
        lcd.write('0' + tens);
    }
    // print ones
    uint8_t const ones = value % 10;
    lcd.write('0' + ones);
}

void lcd_print_temperature(float c)
{
    if (isnan(c)) {
        lcd.print(F(" NaN "));
    } else if (isinf(c)) {
        lcd.print(F("Infin"));
    } else if (c < -9.95f) {
        lcd.print(F("---.-"));
    } else if (c > 99.95f) {
        lcd.print(F("+++.+"));
    } else {
        // Print with one decimal place as a fixed five-character field: "%3d.%1d"
        int16_t temp_int = static_cast<int16_t>(c * 10.0f + (c >= 0.0f ? 0.5f : -0.5f));
        int16_t whole = temp_int / 10;
        uint8_t frac = static_cast<uint8_t>(abs(temp_int % 10));

        // produce three chars for the whole part (width 3, sign included)
        // Note: these characters must be signed for lcd.print().
        char w0, w1, w2;
        int abs_whole = (whole < 0) ? -whole : whole;  //TODO abs() ?
        char sign = (whole < 0) ? '-' : ' ';

        if (abs_whole >= 10) {
            // two whole digits
            w0 = sign;
            w1 = static_cast<char>('0' + (abs_whole / 10) % 10);
            w2 = static_cast<char>('0' + (abs_whole % 10));
        } else {
            // one whole digit: pad on the left, put sign in middle position if negative
            w0 = ' ';
            w1 = sign;
            w2 = static_cast<char>('0' + abs_whole);
        }

        // write the fixed-format numeric field "wd0wd1wd2.frac"
        lcd.print(w0);
        lcd.print(w1);
        lcd.print(w2);
        lcd.print('.');
        lcd.print(frac);
    }
    ////lcd.print(F("\xDFC")); // Degree symbol
    lcd.write('\xDF'); // Avoid 8-bit integer
    lcd.write('C');
}

void check_relay_error_and_fault()
{
    // The PCF8574 library sets an internal _error value. lastError() clears
    // it when read. This helper reads and clears the error state — subsequent
    // checks won't see the same error unless it reoccurs.
    int err = relays.lastError();
    if (err != PCF8574_OK)
    {
        Serial.print(F("PCF8574 error: "));
        Serial.println(err);
        lcd.clear();
        lcd.print(F("FAULT: RELAY I2C"));
        lcd.setCursor(0, 1);
        lcd.print(F("Code "));
        lcd.print(err, HEX);
        faultmenu_enter();
    }
}
