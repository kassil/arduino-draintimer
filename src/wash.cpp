#include "wash.h"
#include "heating.h"
#include "main.h"
#include "utils.h"
#include <Arduino.h>
#include <Keypad_I2C.h>
#include <LiquidCrystal_I2C.h>
#include <PCF8574.h>
#include <stdint.h>

enum class Stage {
    Fill,
    Pump,
    Drain,
};

static void (*substate_loop)(void);
// Clock time when we paused
static unsigned long pause_millis;
// Clock time when the current stage will be complete.
static unsigned long end_millis;
// Number of WASH stages
static uint8_t n_soap;
// Number of RINSE stage
static uint8_t n_rinse;
// Stage number (cumulative count of WASHes and RINSEs)
// Ascending range 1 .. n_soap + n_rinse
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
// Set substate_loop and redraw the LCD.
static void cycle_enter();
static void wash_enter();
static void rinse_enter();
static void drain_enter();

static void print_cycle_wash();
static void print_cycle_rinse();
static void print_stage(Stage stage);

static void turn_all_off();

constexpr unsigned long FILL_TIME_MS = 5000;

void delay_cycle_enter(unsigned long delay, uint8_t n_soap_, uint8_t n_rinse_)
{
    Serial.print(F("Start D:"));
    Serial.print(delay); Serial.print(F(", W:"));
    Serial.print(n_soap_ ); Serial.print(F(", R:"));
    Serial.print(n_rinse_); Serial.println();
    n_soap = n_soap_;
    n_rinse = n_rinse_;
    i_cycle = 1;
    if (delay)
    {
        // Turn all off?
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
        turn_all_off();
        print_remain(now);
    }
}

void cycle_enter()
{
    // Ensure pilot is on, then enable the other relays.
    pilot_on();
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
        Serial.print(F("Paused"));
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
        // Pilot on again
        // TODO Turn on relays
        pilot_on();
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
        Serial.print(F("Cancelled"));
        // Ensure pilot is turned off when cancelling
        pilot_off();
    }
}

void wash_enter()
{
    // Enable fill solenoid.
    relays.write8(~(1<<Relays::FillSolenoid));
    // LCD
    print_cycle_wash();
    print_stage(Stage::Fill);
    const auto now = millis();
    end_millis = now + FILL_TIME_MS;
    substate_loop = fill_loop;
}

void rinse_enter()
{
    // Enable fill solenoid.
    relays.write8(~(1<<Relays::FillSolenoid));
    // LCD
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
        relays.write8(~(1<<Relays::WashMotor));
        if (i_cycle <= n_soap)
        {
            // LCD
            print_cycle_wash();
            dispense_init();  // Enable dispenser
            end_millis = now + 6000;
        }
        else
        {
            // LCD
            print_cycle_rinse();
            end_millis = now + 4000;
        }
        substate_loop = pump_loop;
        // LCD
        print_stage(Stage::Pump);
    }
    else
    {
        // Fill solenoid
        relays.write8(~(1<<Relays::FillSolenoid));
        // LCD
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
        // Decide controls: wash motor, heater, dispenser
        auto relayState = relays.valueOut() & ~(1<<Relays::WashMotor);
        // Control the heater
        if (heating_loop(relayState & (1<<(Relays::HeaterL))) == LOW)
        {
            // Heater ON (active low) - clear the heater bits (active low)
            relayState &= ~((1 << Relays::HeaterL) | (1 << Relays::HeaterN));
        }
        else
        {
            // Heater OFF
            relayState |= (1 << Relays::HeaterL) | (1 << Relays::HeaterN);
        }

        if (i_cycle <= n_soap)
        {
            // Wash: Dispense soap
            auto temp_adc = analogRead(0); // TODO Average
            if(dispense_loop(temp_adc) == LOW)
            {
                relayState &= ~(1 << Relays::Dispenser);  // On
            }
            else
            {
                relayState |= (1 << Relays::Dispenser);  // Off
            }
        }
        relays.write8(relayState);
        print_remain(now);
    }
}

void drain_enter()
{
    // Start draining
    relays.write8(~(1<<Relays::DrainMotor));
    // LCD
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
            turn_all_off();
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
        // Drain
        relays.write8(~(1<<Relays::DrainMotor));
        print_remain(now);
    }
}

void cycle_complete_loop()
{
    // Wait for user, then return to main menu
    turn_all_off();
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

static void turn_all_off()
{
    relays.write8(0xFF); // All relays off (active low)
    pilot_off();
}
