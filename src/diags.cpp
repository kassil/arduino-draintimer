#include "diags.h"
#include "main.h"
#include "menu.h"
#include "heating.h"
#include "utils.h"
#include <Arduino.h>
#include <Keypad_I2C.h>
#include <LiquidCrystal_I2C.h>
#include <PCF8574.h>
#include <avr/wdt.h>
#include <stdint.h>

// Menu callback
static void diagsmenu_select(uint8_t menu_idx);
static void diags_heating_enter();
static void diags_heating_loop();
static void diags_relay_enter();
static void diags_relay_loop();
static void print_temperature(uint8_t const row);

static constexpr byte menu_n_items = 4;
static const char menu_labels_0[] PROGMEM = "to Main Menu";
static const char menu_labels_1[] PROGMEM = "Heating";
static const char menu_labels_2[] PROGMEM = "Watchdog Test";
static const char menu_labels_3[] PROGMEM = "Relays Test";
static const char *const menu_labels[menu_n_items] PROGMEM = {
    menu_labels_0,
    menu_labels_1,
    menu_labels_2,
    menu_labels_3,
};
constexpr auto relay_n = 6;
static const char relay_lbl_0[] PROGMEM = "Fill Solenoid";
static const char relay_lbl_1[] PROGMEM = "Dispenser";
static const char relay_lbl_2[] PROGMEM = "Wash Motor";
static const char relay_lbl_3[] PROGMEM = "Drain Motor";
static const char relay_lbl_4[] PROGMEM = "Heater L";
static const char relay_lbl_5[] PROGMEM = "Heater N";
static const char *const relay_lbls[relay_n] PROGMEM = {
    relay_lbl_0,
    relay_lbl_1,
    relay_lbl_2,
    relay_lbl_3,
    relay_lbl_4,
    relay_lbl_5,
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
        diags_heating_enter();
    }
    else if (menu_idx == 2)
    {
        lcd.clear();
        lcd.print(F("Watchdog Test"));
        Serial.println(F("Watchdog Test"));
        // Intentionally do not reset the watchdog, so it will trigger
        // and reset the MCU in about one second.
        while (true)
        { /*wait*/ }
    }
    else if (menu_idx == 3)
    {
        // Go into a loop where we cycle relays and display
        // temperature until the user presses '#'.
        diags_relay_enter();
    }
    else
    {
        // Invalid menu selection
        Serial.print(F("dm inv "));
        Serial.println(menu_idx);
    }
}

void diags_heating_enter()
{
    relays.write8(0xFF); // All relays off (active low)
    // Start controlling the heater
    heating_init();
    dispense_init();
    pilot_on();
    lcd.clear();
    lcd.print(F("              # Exit"));
    lcd.setCursor(0, 1);
    lcd.print(F("ADC0"));
    loop_function = diags_heating_loop;
}

void diags_heating_loop()
{
    // Show raw ADC and temperature (one decimal)
    static uint8_t last_time = 0;
    uint8_t now = millis() / 1000;
    if (now != last_time)
    {
        last_time = now;
        print_temperature(0);
    }

    auto relayState = relays.valueOut();
    //Serial.print(F("R:"));
    //Serial.println(relayState, BIN);
    // Control the heater
    if (heating_loop(relayState & (1<<(Relays::HeaterL))) == LOW)
    {
        // Heater ON (active low)
        relayState &= ~((1 << Relays::HeaterL) | (1 << Relays::HeaterN));
    }
    else
    {
        // Heater OFF
        relayState |= (1 << Relays::HeaterL) | (1 << Relays::HeaterN);
    }
    // Wash: Dispense soap
    // Problem: It takes awhile to get our first analog sample. In that time the heater
    // is off.  The dispenser thinks the water is warm.
    uint16_t temp_raw;
    if (calc_analog_mean(0, temp_raw) && dispense_loop(temp_raw)== LOW)
    {
        relayState &= ~(1 << Relays::Dispenser);  // On
    }
    else
    {
        relayState |= (1 << Relays::Dispenser);  // Off
    }
    relays.write8(relayState);

    lcd.setCursor(0, 0);
    lcd.print(relayState & (1<<(Relays::HeaterL)) ? 'h' : 'H');
    lcd.print(relayState & (1<<(Relays::Dispenser)) ? 'd' : 'D');

    // Service keypad
    char const customKey = customKeypad.getKey();
    if (customKey == '#')
    {
        // Exit this mode
        relays.write8(0xFF); // All relays off (active low)
        pilot_off();
        diags_enter();
    }
}

void diags_relay_enter()
{
    pilot_on();
    lcd.clear();
    lcd.print(F("All Sensors   # Exit"));
    for (uint8_t row = 0; row < 2; ++row) {
        lcd.setCursor(0, row + 1);
        lcd.print(F("ADC"));
    }
    loop_function = diags_relay_loop;
}

void diags_relay_loop()
{
    // Show raw ADC and temperature (one decimal)
    for (uint8_t row = 0; row < 2; ++row) {
        print_temperature(row);
    }
    // Cycle relays
    static uint8_t current_relay = 0;
    static unsigned long last_switch_millis = 0;
    unsigned long const now = millis();
    if (now - last_switch_millis >= 1000)
    {
        last_switch_millis = now;
        relays.write8(~(1 << current_relay)); // Activate one relay at a time (

        // Fetch pointer from program memory
        auto const lbl = static_cast<char const*>(pgm_read_ptr(relay_lbls + current_relay));
        lcd.setCursor(0, 3);
        lcd.print(reinterpret_cast<__FlashStringHelper const *>(lbl));
        // Clear end of line
        for (uint8_t i = strlen_P(lbl); i < LCD_N_COLS - 1; ++i)
            lcd.print(' ');

        current_relay = (current_relay + 1) % relay_n;
    }

    // Service keypad
    char const customKey = customKeypad.getKey();
    if (customKey == '#')
    {
        // Exit this mode
        relays.write8(0xFF); // All relays off (active low)
        pilot_off();
        diags_enter();
    }
}

// Show raw ADC and temperature (one decimal)
void print_temperature(uint8_t const row)
{
    uint16_t adc;
    if (!calc_analog_mean(row, adc)) {
        return;
    }
    lcd.setCursor(0, row + 1);
    lcd.print(F("ADC"));
    lcd.print(row);
    lcd.setCursor(5, row + 1);
    lcd_print_right_justify(adc, 5);
    // Print temperature in Celsius
    float c = adc_to_celsius(adc);
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
