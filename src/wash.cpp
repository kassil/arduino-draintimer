#include "wash.h"
#include "door.h"
#include "heating.h"
#include "main.h"
#include "utils.h"
#include "temperature.h"
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
static void cycle_wait_door();
static void delay_check_door_loop();
static void fill_loop();
static void pump_loop();
static void drain_loop();
static void cycle_complete_loop();
static void paused_loop();
static void print_remain();

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

static constexpr unsigned long FILL_TIME_MS = 10000;
static constexpr unsigned long WASH_TIME_MS = 20000;
static constexpr unsigned long RINSE_TIME_MS = 8000;
static constexpr unsigned long DRAIN_TIME_MS = 7000;
char const relayShortLabel[6] PROGMEM = {'F','W','S','D','L','N'};

static uint32_t last_display_time;

void delay_cycle_enter(uint16_t delay_minutes, uint8_t n_soap_, uint8_t n_rinse_)
{
    Serial.print(F("Start D:"));
    Serial.print(delay_minutes); Serial.print(F(" mins, W:"));
    Serial.print(n_soap_ ); Serial.print(F(", R:"));
    Serial.print(n_rinse_); Serial.println();
    n_soap = n_soap_;
    n_rinse = n_rinse_;
    i_cycle = 1;

    if (delay_minutes > 0)
    {
        // Turn all off?
        lcd.clear();
        lcd.print(F("Delay Start"));
        // Briefly enable pilot so we can verify door state (debounced)
        pilot_on();
        substate_loop = delay_check_door_loop;
        end_millis = millis() + delay_minutes * 60ul * 1000ul;
        last_display_time = millis() - 500; // Force display update
    }
    else
    {
        // No delay
        // (Turns on pilot again)
        cycle_enter();
    }
    loop_function = cycle_loop;
}

void delay_loop()
{
    // keep sampling the door while we're in delay
    door_tick();
    const auto now = millis();
    if (now >= end_millis)
    {
        // Delay finished
        cycle_enter();
    }
    else
    {
        // Delay washing
        turn_all_off();
    }
}

// After briefly turning the pilot on we wait for the door to be closed. If the
// user cancels (#) we abort the cycle. If the user forces continue (*) we
// either return to the delay wait state (pilot off) or start immediately if
// the delay has expired.
void delay_check_door_loop()
{
    // sample door each iteration
    door_tick();

    // If the door has just become stably closed or is closed now, proceed
    if (door_was_closed() || door_is_closed()) {
        // If there's still time remaining before the scheduled start, turn the
        // pilot off to conserve power and return to the delay wait loop.
        if (millis() < end_millis) {
            pilot_off();
            substate_loop = delay_loop;
        } else {
            // no remaining delay, start immediately
            cycle_enter();
        }
        return;
    }

    // allow user input while waiting
    char const customKey = customKeypad.getKey();
    if (customKey == '#') {
        // Cancel the cycle
        lcd.clear();
        lcd.print(F("Cycle cancelled"));
        lcd.setCursor(0, 1);
        lcd.print(F("Press *"));
        loop_function = cycle_complete_loop;
        Serial.print(F("Cancelled"));
        pilot_off();
        return;
    } else if (customKey == '*') {
        // User forces continue: if there is still a delay, turn pilot off and
        // return to delay loop; otherwise start immediately.
        if (millis() < end_millis) {
            pilot_off();
            substate_loop = delay_loop;
        } else {
            cycle_enter();
        }
        return;
    }

    // Prompt user to close the door if still open
    lcd.setCursor(0, 2);
    lcd.print(F("Please Close Door"));
    lcd.setCursor(0, 3);
    lcd.print(F("# Cancel  * Continue"));
}

void cycle_enter()
{
    // Ensure pilot is on, then enable the other relays.
    end_millis = millis() + 1000;
    // Ensure pilot is on, then enable the other relays.
    pilot_on();
    last_display_time = millis() - 500; // Force display update

    loop_function = cycle_wait_door;
}

void cycle_wait_door()
{
    // sample the door while waiting
    door_tick();
    const auto now = millis();

    if (!door_is_open())
    {
        // Door closed: proceed to the appropriate enter handler
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
        return;
    }

    // If we've reached the grace period and the door is still open, prompt
    if (now >= end_millis)
    {
        auto customKey = customKeypad.getKey();
        if (customKey == '#')
        {
            // Cancel the cycle
            lcd.clear();
            lcd.print(F("Cycle cancelled"));
            lcd.setCursor(0, 1);
            lcd.print(F("Press *"));
            loop_function = cycle_complete_loop;
            Serial.print(F("Cancelled"));
            // Ensure pilot is turned off when cancelling
            pilot_off();
            return;
        }
        else if (customKey == '*')
        {
            // User forces continue: move back to normal cycle processing
            loop_function = cycle_loop;
            Serial.print(F("Continue"));
            return;
        }

        // Prompt user to close the door
        lcd.setCursor(0, 2);
        lcd.print(F("Please Close Door"));
        lcd.setCursor(0, 3);
        lcd.print(F("# Cancel  * Continue"));
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

    // Display relay states and temperature
    lcd.setCursor(0, 3);
    uint8_t const relayState = relays.valueOut();
    for(uint8_t i = 0; i < 6; ++i) {
        char const ch = (relayState & (1<<i)) ? ' ' : static_cast<char>(pgm_read_byte(&relayShortLabel[i]));
        lcd.write(ch);
    }

    uint32_t const now = millis();
    if ((now - last_display_time) >= 500UL) {
        last_display_time = now;
        // Print time remaining
        print_remain();
        // Print temperature beside time
        uint16_t adc;
        if (temperature_get_adc_mean(adc)) {
            float const temp_c = temperature_adc_to_celsius(adc);
            lcd_print_temperature(temp_c);
        }
    }
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
            end_millis = now + WASH_TIME_MS;
        }
        else
        {
            // LCD
            print_cycle_rinse();
            end_millis = now + RINSE_TIME_MS;
        }
        substate_loop = pump_loop;
        // LCD
        print_stage(Stage::Pump);
    }
    else
    {
        // Fill solenoid
        relays.write8(~(1<<Relays::FillSolenoid));
    }
}

void pump_loop()
{
    const auto now = millis();
    if (now >= end_millis)
    {
        drain_enter();  // Finished
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
            if(dispense_loop() == LOW)
            {
                relayState &= ~(1 << Relays::Dispenser);  // On
            }
            else
            {
                relayState |= (1 << Relays::Dispenser);  // Off
            }
        }
        relays.write8(relayState);
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
    end_millis = now + DRAIN_TIME_MS;
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

void print_remain()
{
    uint32_t now = millis();
    uint32_t remain_ms = end_millis - now;
    // Ignore negative subtraction, since clock wraps around.
    uint32_t whole_s = remain_ms / 1000ul;
    auto const hours = static_cast<uint8_t>(whole_s / 3600u);
    auto const mins  = static_cast<uint8_t>((whole_s / 60u) % 60u);
    auto const secs  = static_cast<uint8_t>(whole_s % 60u);
    auto const tenths   = static_cast<uint8_t>((remain_ms % 1000u) / 100u);

    lcd.setCursor(0, 2);
    // Hours: space-padded to width 2
    lcd_print_right_justify_2d(hours, ' ');
    lcd.print(':');
    // Minutes: zero-padded to 2 digits
    lcd_print_right_justify_2d(mins, '0');
    lcd.print(':');
    // Seconds: zero-padded to 2 digits
    lcd_print_right_justify_2d(secs, '0');
    lcd.print(':');
    // Tenths: zero-padded to 1 digit
    lcd.write('0' + tenths);
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
    switch (stage) {
    case Stage::Fill:
        str = F("Filling  ");
        break;
    case Stage::Pump:
        str = F("Circulate");
        break;
    case Stage::Drain:
        str = F("Draining ");
        break;
    default:
        str = F("?Stage?  ");
    }
    lcd.setCursor(0, 1);
    lcd.print(str);
}

static void turn_all_off()
{
    relays.write8(0xFF); // All relays off (active low)
    pilot_off();
}
