#include "wash.h"
#include "drain_timer.h"
#include <Arduino.h>
#include <Keypad_I2C.h>
#include <LiquidCrystal_I2C.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum class Stage {
    Fill,
    Pump,
    Drain,
};

// static unsigned long start_millis;
static unsigned long end_millis;
static uint8_t n_soap;
static uint8_t n_rinse;

// Periodically call this to service the mode.
static void fill_loop();
static void pump_loop();
static void drain_loop();
static void cycle_complete_loop();
static void print_remain(unsigned long const& now);

// Enter the wash/rinse/drain mode
static void wash_enter();
static void rinse_enter();
static void drain_enter();

static void printStage(Stage stage);
static void lcdPrintLeftJustify(uint16_t value, uint8_t width);

void cycle_enter(uint8_t n_soap_, uint8_t n_rinse_)
{
    n_soap = n_soap_;
    n_rinse = n_rinse_;
    if (n_soap)
    {
        wash_enter();
    }
    else if (n_rinse)
    {
        rinse_enter();
    }
    else
    {
        drain_enter();
    }
}

void wash_enter()
{
    // Start filling
    lcd.clear();
    lcd.print(F("Wash "));
    lcd.print(n_soap);
    printStage(Stage::Fill);
    const auto now = millis();
    end_millis = now + 3000;
    loop_function = fill_loop;
}

void rinse_enter()
{
    // Start filling
    lcd.clear();
    lcd.print(F("Rinse "));
    lcd.print(n_rinse);
    printStage(Stage::Fill);
    const auto now = millis();
    end_millis = now + 3000;
    loop_function = fill_loop;
}

void fill_loop()
{
    const auto now = millis();
    if (now >= end_millis)
    {
        // Start pumping
        lcd.clear();
        if (n_soap)
        {
            lcd.print(F("Wash "));
            lcd.print(n_soap);
            end_millis = now + 6000;
        }
        else
        {
            lcd.print(F("Rinse "));
            lcd.print(n_rinse);
            end_millis = now + 3000;
        }
        printStage(Stage::Pump);
        loop_function = pump_loop;
    }
    else
    {
        print_remain(now);
    }
}

void pump_loop()
{
    const auto now = millis();
    if (now >= end_millis)
    {
        drain_enter();
    }
    else
    {
        //TODO Heat
        print_remain(now);
    }
}

void drain_enter()
{
    // Start draining
    lcd.clear();
    if (n_soap)
    {
        lcd.print(F("Wash "));
        lcd.print(n_soap);
    }
    else if (n_rinse)
    {
        lcd.print(F("Rinse "));
        lcd.print(n_rinse);
    }
    else
    {
        // Not part of a wash/rinse cycle
    }
    printStage(Stage::Drain);
    const auto now = millis();
    end_millis = now + 3000;
    loop_function = drain_loop;
}

void drain_loop()
{
    const auto now = millis();
    if (now >= end_millis)
    {
        // Another cycle?
        if (n_soap > 1)
        {
            n_soap --;
            wash_enter();
        }
        else if (n_rinse > 1)
        {
            n_rinse --;
            rinse_enter();
        }
        else // no more cycles
        {
            // Wait for user
            lcd.clear();
            lcd.print(F("Cycle complete"));
            lcd.setCursor(0, 1);
            lcd.print(F("Press *"));
            loop_function = cycle_complete_loop;
        }
    }
    else
    {
        print_remain(now);
    }
}

void cycle_complete_loop()
{
    // Print CYCLE COMPLETE and hold here
    char const customKey = customKeypad.getKey();
    if (customKey == '*')
    {
        mainmenu_enter();
    }
}

void print_remain(unsigned long const& now)
{
    auto remain = static_cast<unsigned short>((end_millis - now)/100);
    lcd.setCursor(7, 1);
    lcdPrintLeftJustify(remain, 5);
    lcd.print(F("s"));
}

void printStage(Stage stage)
{
    lcd.setCursor(10, 0);
    const __FlashStringHelper* str;
    if (stage == Stage::Fill) {
        str = F("Filling");
    }
    else if (stage == Stage::Pump) {
        str = F("Circulate");
    }
    else if (stage == Stage::Drain) {
        str = F("Draining");
    }
    else {
        str = F("--");
    }
    lcd.setCursor(0, 1);
    lcd.print(F("Remain "));
}

// Print an integer left-justified in a fixed-width field
static void lcdPrintLeftJustify(uint16_t value, uint8_t width) {
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
