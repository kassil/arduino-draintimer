#define LCD_I2C

#include "drain_timer.h"
#include "menu.h"
#include "monitor.h"
#include "my_timer.h"

#include <Arduino.h>
#include <Keypad_I2C.h>
#include <Keypad.h> // GDY120705
#ifdef LCD_I2C
#include <LiquidCrystal_I2C.h>
#else
#include <LiquidCrystal.h>
#endif
#include <Wire.h>
#include <stdint.h>

constexpr uint8_t KPD_SLAVE = 0x20;
// Addr Vend   A2 A1 A0
// 27   NXP    Hi Hi Hi
// 20   NXP    Lo Lo Lo

constexpr byte KPD_ROWS = 4; // Dimensions of matrix
constexpr byte KPD_COLS = 4; //
// define the symbols on the buttons of the keypads
char const hexaKeys[KPD_ROWS][KPD_COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};
byte rowPins[KPD_ROWS] = {0, 1, 2, 3}; // connect to the row pinouts of the keypad
byte colPins[KPD_COLS] = {4, 5, 6, 7}; // connect to the column pinouts of the keypad
Keypad_I2C customKeypad(makeKeymap(hexaKeys), rowPins, colPins, KPD_ROWS, KPD_COLS, KPD_SLAVE);

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
#ifdef LCD_I2C
constexpr uint8_t lcd_slave = 0x27;
LiquidCrystal_I2C lcd(lcd_slave, LCD_N_COLS, LCD_N_ROWS);
#else
constexpr uint8_t rs = 4, en = 5, d4 = 8, d5 = 9, d6 = 10, d7 = 11;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
#endif

unsigned long next_draw_time = 0;

// What function we call in our loop.  This changes with the state.
void (*loop_function)();

const byte names_n_items = 3;
const char names_0[] PROGMEM = "Set Off Time";
const char names_1[] PROGMEM = "Set On Time";
const char names_2[] PROGMEM = "Exit";
const char *const names_labels[] PROGMEM = {
    names_0,
    names_1,
    names_2,
};

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH); // Turn the LED on.

    Wire.begin(); // GDY200622
    Serial.begin(115200);

    // set up the LCD's number of columns and rows:
#ifdef LCD_I2C
    lcd.init();
    lcd.backlight();
#else
    lcd.begin(LCD_N_COLS, LCD_N_ROWS);
#endif

    customKeypad.begin(); // GDY120705

    while (!Serial)
    { /*wait*/
    }

    timer_init();
    monitor_enter();

    Serial.print(F("Boot "));
    Serial.print(switch_millis);
    Serial.println(F(" ms"));
    digitalWrite(LED_BUILTIN, LOW); // Turn the LED off.
}

void loop()
{
    loop_function();

    // Update the timer. Do this in all states.
    timer_loop();
}

void monitor_enter()
{
    loop_function = monitor_loop;
    next_draw_time = millis();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(g_switch_state ? F("On ") : F("Off"));
    lcd.setCursor(0, 1);
    lcd.print(F("LED:"));
}

void monitor_loop()
{
    // Keypad
    char const customKey = customKeypad.getKey();
    if (customKey != NO_KEY)
    {
        Serial.println(customKey);
    }

    if (customKey == '*')
    {
        // Switch to menu mode
        menu_enter(names_n_items, names_labels);
        return;
    }
    else if (customKey == 'A')
    {
        // Toggle LED
        uint8_t const ledState = !digitalRead(LED_BUILTIN);
        digitalWrite(LED_BUILTIN, ledState);
        lcd.setCursor(17, 1);
        lcd.print(ledState ? F("On ") : F("Off"));
    }

    bool update_time_display = false;
    auto const now = millis();
    if (now >= next_draw_time)
    {
        update_time_display = true;
        next_draw_time = (now - (now % 1000)) + 1000;
    }

    if (update_time_display || g_update_state)
    {
        auto elapsed = now - switch_millis;
        if (g_update_state)
        {
            g_update_state = false;
            lcd.setCursor(0, 0);
            lcd.print(g_switch_state ? F("On ") : F("Off"));
        }

        if (update_time_display)
        {
            Serial.print(now);
            Serial.print(F("\telapsed "));
            Serial.print(elapsed);
            Serial.print(F(" next "));
            Serial.print(next_draw_time);
            Serial.println();
        }

        // Display count down time
        lcd.setCursor(LCD_N_COLS - 8 - 3, 0);
        print_hms_time(lcd, timer_get_duration() - elapsed);
    }
}

void monitor_draw()
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(g_switch_state ? F("On ") : F("Off"));
    lcd.setCursor(0, 1);
    lcd.print(F("LED:"));
}
