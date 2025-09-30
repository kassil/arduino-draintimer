#include "number_entry.h"
#include "drain_timer.h"
#include "monitor.h"
#include "my_timer.h"
#include <Arduino.h>
#include <Keypad_I2C.h>
#include <LiquidCrystal_I2C.h>
#include <stdint.h>

uint8_t g_number_entry_state;
uint8_t g_number_entry_column;
char g_number_entry_string[21];

void number_entry_init(int target_idx)
{
    loop_function = number_entry_loop;
    g_number_entry_state = target_idx;
    g_number_entry_column = 0;

    unsigned long milliseconds = timer_get_duration(g_number_entry_state);
    constexpr auto TEN_DAYS = 10ul * 24 * 60 * 60 * 1000ul;
    milliseconds = min(TEN_DAYS, milliseconds);
    unsigned long seconds = milliseconds / 1000;
    snprintf(g_number_entry_string, sizeof(g_number_entry_string), "%lud %02lu:%02lu:%02lu",
             seconds / 86400,
             (seconds / 3600) % 24,
             (seconds / 60) % 60,
             seconds % 60);

    lcd.clear();
    lcd.print(F("Enter Time:"));
    lcd.setCursor(0, 1);
    lcd.print(g_number_entry_string);
    lcd.cursor();
    lcd.blink();
    lcd.setCursor(0, 1);
}

static bool number_entry_check()
{
    char const& c = g_number_entry_string[g_number_entry_column];
    return '0' <= c && c <= '9';
}

void number_entry_loop()
{
    char const customKey = customKeypad.getKey();
    if (customKey == NO_KEY)
    {
        return;
    }

    if ('0' <= customKey && customKey <= '9' && g_number_entry_column < 11)
    {
        if (!number_entry_check())
        {
            Serial.print(F("Number entry wrong column "));
            Serial.println(g_number_entry_column);
            return;
        }
        lcd.print(customKey);
        g_number_entry_string[g_number_entry_column] = customKey;

        while (g_number_entry_column < 10)
        {
            g_number_entry_column++;
            if (number_entry_check())
            {
                break; // Move cursor here
            }
        }
        lcd.setCursor(g_number_entry_column, 1);
    }
    else if (customKey == 'B') // Left
    {
        while (g_number_entry_column > 0)
        {
            g_number_entry_column--;
            if (number_entry_check())
            {
                break; // Move cursor here
            }
        }
        lcd.setCursor(g_number_entry_column, 1);
    }
    else if (customKey == 'C') // Right
    {
        while (g_number_entry_column < 10)
        {
            g_number_entry_column++;
            if (number_entry_check())
            {
                break; // Move cursor here
            }
        }
        lcd.setCursor(g_number_entry_column, 1);
    }
    else if (customKey == '*')
    {
        Serial.print(F("Number entry: "));
        Serial.println(g_number_entry_string);
        unsigned short days, hours, mins, secs;
        int n_elts = sscanf_P(g_number_entry_string, PSTR("%hud %02hu:%02hu:%02hu"),
                              &days, &hours, &mins, &secs);
        if (n_elts == 4)
        {
            unsigned long duration_ms = 1000ul * (secs + (60ul * mins + (60ul * (hours + 24ul * (unsigned long)days))));
            Serial.println(duration_ms);
            if (duration_ms >= 1000)
            {
                timer_set_duration(g_number_entry_state, duration_ms);
            }
        }
        else
        {
            Serial.print(F("PARSE FAIL "));
            Serial.println(n_elts);
        }
        // Back to monitor screen
        lcd.noBlink();
        lcd.noCursor();
        monitor_enter();
    }
}
