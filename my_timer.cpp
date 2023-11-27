#include "drain_timer.h"
#include "my_timer.h"
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

struct SwitchDetails
{
    unsigned long duration;
};

SwitchDetails switch_details[2] =
{
    { .duration = 10ul * 1000, },
    { .duration = 2ul*24*60*60 * 1000, },
};

constexpr uint8_t relay_pin_first = 4;
constexpr uint8_t relay_pin_last  = 8;

unsigned long switch_millis;
uint8_t g_switch_state;
bool g_update_time_display;
bool g_update_state;


void timer_init()
{
    for (byte pin = relay_pin_first; pin != relay_pin_last; ++pin)
    {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }

    switch_millis = millis();
    g_update_state = true;
    g_switch_state = 0;
}

void timer_loop()
{
    auto now = millis();
    auto elapsed = now - switch_millis;
    //if (switch_millis + switch_details[g_switch_state].duration < now)
    if (elapsed > switch_details[g_switch_state].duration)
    {
        // Don't accumulate latency
        switch_millis += switch_details[g_switch_state].duration;
        g_switch_state = g_switch_state ? 0 : 1;
        g_update_state = true;

        digitalWrite(relay_pin_first, g_switch_state ? HIGH : LOW);

        Serial.print(now);
        Serial.print(F("\tswitch_millis "));
        Serial.print(switch_millis);
        Serial.print(F("\tWake + "));
        Serial.print(switch_details[g_switch_state].duration);
        Serial.print(F("\t= "));
        Serial.print(switch_millis + switch_details[g_switch_state].duration);
        Serial.println();
    }
}

unsigned long timer_get_duration()
{
    return switch_details[g_switch_state].duration;
}

void print_hms_time(Print& target, unsigned long milliseconds)
{
    constexpr auto TEN_DAYS = 10ul * 24 * 60 * 60 * 1000ul;
    milliseconds = min(TEN_DAYS, milliseconds);
    char buffer[13];
    unsigned long seconds = milliseconds / 1000;
    snprintf(buffer, sizeof(buffer), "%lud %2lu:%02lu:%02lu",
        seconds / 86400,
        (seconds / 3600) % 24,
        (seconds / 60) % 60,
        seconds % 60);
    target.print(buffer);
}
