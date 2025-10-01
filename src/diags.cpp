#include "diags.h"
#include "drain_timer.h"
#include "menu.h"
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
static const char menu_labels_2[] PROGMEM = "--";
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
    //TODO We could break this up into iterations
    uint16_t a;
    a = analog_mean(A0, 1024);
    lcd.setCursor(5, 1);
    lcd_print_right_justify(a, 5);
    a = analog_mean(A1, 1024);
    lcd.setCursor(5, 2);
    lcd_print_right_justify(a, 5);

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
