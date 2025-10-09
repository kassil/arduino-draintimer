#include "diags.h"
#include "main.h"
#include "menu.h"
#include "heating.h"
#include "utils.h"
#include <Arduino.h>
#include <Keypad_I2C.h>
#include <LiquidCrystal_I2C.h>
#include <stdint.h>

// Menu callback
static void diagsmenu_select(uint8_t menu_idx);
static void diags_sensors_enter();
static void diags_sensor_loop();
static uint16_t analog_mean(uint8_t pin, uint16_t samples);

static constexpr byte menu_n_items = 6;
static const char menu_labels_0[] PROGMEM = "to Main Menu";
static const char menu_labels_1[] PROGMEM = "Sensors";
static const char menu_labels_2[] PROGMEM = "Watchdog Test";
static const char menu_labels_3[] PROGMEM = "--";
static const char menu_labels_4[] PROGMEM = "--";
static const char menu_labels_5[] PROGMEM = "--";
static const char *const menu_labels[menu_n_items] PROGMEM = {
    menu_labels_0,
    menu_labels_1,
    menu_labels_2,
    menu_labels_3,
    menu_labels_4,
    menu_labels_5,
};

void diags_enter()
{
    menu_enter(menu_n_items, menu_labels, diagsmenu_select);
}

void diagsmenu_select(uint8_t menu_idx)
{
    Serial.print(F("dm sel "));
    Serial.println(menu_idx);
    if (menu_idx == 0)
    {
        mainmenu_enter();
    }
    else if (menu_idx == 1)
    {
        diags_sensors_enter();
    }
    else if (menu_idx == 2)
    {
        Serial.println(F("Watchdog Test"));
        // Intentionally do not reset the watchdog, so it will trigger
        // and reset the MCU in about one second.
        while (true)
        { /*wait*/ }
    }
    else if (menu_idx == 3)
    {
    }
    else if (menu_idx == 4)
    {
    }
    else if (menu_idx == 5)
    {
    }
    else if (menu_idx == 6)
    {
    }
    else
    {
        // Invalid menu selection
        Serial.print(F("dm inv "));
        Serial.println(menu_idx);
    }
}

void diags_sensors_enter()
{
    lcd.clear();
    lcd.print(F("Sensors       # Exit"));
    lcd.setCursor(0, 1);
    lcd.print(F("ADC0"));
    loop_function = diags_sensor_loop;
}

void diags_sensor_loop()
{
    // Show raw ADC and temperature (one decimal)
    for (uint8_t row = 0; row < 2; ++row) {
        lcd.setCursor(0, row + 1);
        lcd.print(F("ADC"));
        lcd.print(row);
        uint16_t a = analog_mean(row, 1024);
        lcd.setCursor(5, row + 1);
        lcd_print_right_justify(a, 5);
        // Print temperature in Celsius
        float c = adc_to_celsius(a);
        lcd.setCursor(12, row + 1);
        if (c < -9.95f) {
            lcd.print(F("---"));
        } else if (c > 99.95f) {
            lcd.print(F("+++"));
        } else {
            // Print with one decimal place
            int16_t temp_int = static_cast<int16_t>(c * 10.0f + (c >= 0.0f ? 0.5f : -0.5f));
            int16_t whole = temp_int / 10;
            int16_t frac = abs(temp_int % 10);
            lcd.print(whole);
            lcd.print(F("."));
            lcd.print(frac);
            lcd.print(F("C"));
        }
    }
    // Service keypad
    char const customKey = customKeypad.getKey();
    if (customKey == '#')
    {
        // Exit this mode
        diags_enter();
    }
}

uint16_t analog_mean(uint8_t const pin, uint16_t const samples) {
    // Given 32-bit accumulator, we bound samples such that
    // 1024 x samples < 2^32
    // log(1024) + log(samples) < 32
    // log(samples) < 22
    // samples < 4194304
    uint32_t sum = 0;
    for (uint16_t i = 0; i < samples; i++) {
        sum += analogRead(pin);
    }
    return static_cast<uint16_t>(sum / samples);
}
