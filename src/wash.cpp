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
static void (*substate_loop)(void);
static unsigned long pause_millis;
static unsigned long end_millis;
static uint8_t n_soap;
static uint8_t n_rinse;
static uint8_t i_cycle;

// Periodically call this to service the mode.
static void delay_loop();
static void cycle_loop();
static void fill_loop();
static void pump_loop();
static void drain_loop();
static void cycle_complete_loop();
static void paused_loop();
static void print_remain(unsigned long const& now);

// Enter the wash/rinse/drain mode
static void cycle_enter();
static void wash_enter();
static void rinse_enter();
static void drain_enter();

static void print_cycle_wash();
static void print_cycle_rinse();
static void print_stage(Stage stage);
static void lcd_print_left_justify(uint16_t value, uint8_t width);
static void lcd_print_right_justify(uint16_t value, uint8_t width);

constexpr unsigned long FILL_TIME_MS = 2500;

void delay_cycle_enter(unsigned long delay, uint8_t n_soap_, uint8_t n_rinse_)
{
    n_soap = n_soap_;
    n_rinse = n_rinse_;
    i_cycle = 1;
    if (delay)
    {
        lcd.clear();
        lcd.print(F("Delay Start"));
        substate_loop = delay_loop;
        end_millis = millis() + delay;
    }
    else
    {
        // No delay
        cycle_enter();
    }
    loop_function = cycle_loop;
}

void delay_loop()
{
    const auto now = millis();
    if (now >= end_millis)
    {
        // Delay finished
        cycle_enter();
    }
    else
    {
        print_remain(now);
    }
}

void cycle_enter()
{
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

void cycle_loop()
{
    // For all cycles except paused_loop and cycle_complete_loop
    // Service keypad
    char const customKey = customKeypad.getKey();
    if (customKey == '#')
    {
        lcd.setCursor(0, 3);
        lcd.print(F("* Resume    # Cancel"));
        pause_millis = millis();
        loop_function = paused_loop;
        return;
    }
    // Run the cycle
    substate_loop();
}

void paused_loop()
{
    char const customKey = customKeypad.getKey();
    if (customKey == '*')
    {
        // Resume from pause
        end_millis += millis() - pause_millis;
        lcd.setCursor(0, 3);
        lcd.print(F("                    "));
        loop_function = cycle_loop;
    }
    else if (customKey == '#')
    {
        //TODO Hold button for > 300 ms?
        // Wait for user
        lcd.clear();
        lcd.print(F("Cycle cancelled"));
        lcd.setCursor(0, 1);
        lcd.print(F("Press *"));
        loop_function = cycle_complete_loop;
    }
}

void wash_enter()
{
    // Start filling
    print_cycle_wash();
    print_stage(Stage::Fill);
    const auto now = millis();
    end_millis = now + FILL_TIME_MS;
    substate_loop = fill_loop;
}

void rinse_enter()
{
    // Start filling
    print_cycle_rinse();
    print_stage(Stage::Fill);
    const auto now = millis();
    end_millis = now + FILL_TIME_MS;
    substate_loop = fill_loop;
}

void fill_loop()
{
    const auto now = millis();
    if (now >= end_millis)
    {
        // Start pumping
        if (i_cycle <= n_soap)
        {
            print_cycle_wash();
            end_millis = now + 4500;
        }
        else
        {
            print_cycle_rinse();
            end_millis = now + 2500;
        }
        print_stage(Stage::Pump);
        substate_loop = pump_loop;
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
    if (i_cycle <= n_soap)
    {
        print_cycle_wash();
    }
    else if (i_cycle <= n_soap + n_rinse)
    {
        print_cycle_rinse();
    }
    else
    {
        // Not part of a wash/rinse cycle
        lcd.clear();
    }
    print_stage(Stage::Drain);
    const auto now = millis();
    end_millis = now + 2500;
    substate_loop = drain_loop;
}

void drain_loop()
{
    const auto now = millis();
    if (now >= end_millis)
    {
        // Another cycle?
        i_cycle++;
        if (i_cycle <= n_soap)
        {
            wash_enter();
        }
        else if (i_cycle <= n_soap + n_rinse)
        {
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
    // Wait for user, then return to main menu
    char const customKey = customKeypad.getKey();
    if (customKey == '*')
    {
        mainmenu_enter();
    }
}

void print_remain(unsigned long const& now)
{
    auto remain = static_cast<unsigned short>((end_millis - now)/100);
    lcd.setCursor(7, 2);
    lcd_print_right_justify(remain, 4);
    lcd.print(F("s"));
}

void print_cycle_wash()
{
    lcd.clear();
    lcd.print(F("Wash "));
    lcd.print(i_cycle);
    lcd.print('/');
    lcd.print(n_soap);
}

void print_cycle_rinse()
{
    lcd.clear();
    lcd.print(F("Rinse "));
    lcd.print(i_cycle - n_soap);
    lcd.print('/');
    lcd.print(n_rinse);
}

void print_stage(Stage stage)
{
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
        str = F("?Stage?");
    }
    lcd.setCursor(0, 1);
    lcd.print(str);
    lcd.setCursor(0, 2);
    lcd.print(F("Remain "));
}

// Print a non-negative integer left-justified in a fixed-width field
static void lcd_print_left_justify(uint16_t value, uint8_t width) {
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
static void lcd_print_right_justify(uint16_t value, uint8_t width) {
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
