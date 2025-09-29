#define LCD_I2C

#include "drain_timer.h"
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
char hexaKeys[KPD_ROWS][KPD_COLS] = {
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

void monitor_init();
void monitor_loop();
void timer_loop();
void menu_init(uint8_t n_items, char const *const *labels);
void menu_loop();
void menu_draw();
void number_entry_init(int target_idx);
void number_entry_loop();

static __FlashStringHelper const *toFSH(char const *progmem_ptr);

unsigned long last_draw_time = 0;

// What function we call in our loop.  This changes with the state.
void (*loop_function)();

struct MenuState
{
    uint8_t cursor_row;
    uint8_t menu_top_row;
    uint8_t n_items;
    char const *const *labels;
};
MenuState menuState;

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
    Serial.begin(9600);

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
    monitor_init();

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

void monitor_init()
{
    loop_function = monitor_loop;
    last_draw_time = millis() - 0x7FFFFFFF;

    monitor_draw();
}

void monitor_loop()
{
    char const customKey = customKeypad.getKey();
    if (customKey != NO_KEY)
    {
        Serial.println(customKey);
    }

    if (customKey == '*')
    {
        menu_init(names_n_items, names_labels);
        return;
    }
    else if (customKey == 'A')
    {
        // Toggle LED
        uint8_t ledState = !digitalRead(LED_BUILTIN);
        digitalWrite(LED_BUILTIN, ledState);
        lcd.setCursor(17, 1);
        lcd.print(ledState ? F("On ") : F("Off"));
    }

    bool update_time_display = false;
    auto now = millis();
    auto elapsed = now - switch_millis;
    if (elapsed - last_draw_time > 1000) // time_update_millis/1000 < elapsed / 1000)
    {
        update_time_display = true;
        last_draw_time = elapsed - (elapsed % 1000);
    }

    if (update_time_display || g_update_state)
    {
        if (g_update_state)
        {
            g_update_state = false;
            lcd.setCursor(0, 0);
            lcd.print(g_switch_state ? F("On ") : F("Off"));
        }

        if (update_time_display)
        {
            Serial.print(now);
            Serial.print(F("\tlast draw "));
            Serial.print(last_draw_time);
            Serial.print(F("\telapsed "));
            Serial.print(elapsed);
            Serial.print(F(" next "));
            Serial.print(last_draw_time + 1000);
            Serial.println();
        }

        // Display count down time
        lcd.setCursor(LCD_N_COLS - 8 - 4, 0);
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
    // lcd.setCursor(0, 2);
    // lcd.setCursor(0, 3);
    // lcd.print(toFSH((char const*) pgm_read_ptr(names_labels + selectedNameIdx)));
}

void menu_init(uint8_t n_items, char const *const *labels)
{
    menuState.cursor_row = 0;
    menuState.menu_top_row = 0;
    menuState.n_items = n_items;
    menuState.labels = labels;
    loop_function = menu_loop;

    menu_draw();
}

void menu_loop()
{
    char const customKey = customKeypad.getKey();
    if (customKey == NO_KEY)
    {
        return;
    }

    Serial.println(customKey);

    if (customKey == 'D')
    { // Down

        if (menuState.cursor_row + 1u < menuState.n_items)
        {
            menuState.cursor_row++;
            if (menuState.menu_top_row + LCD_N_ROWS - 1 < menuState.cursor_row)
                menuState.menu_top_row = max(0, (int8_t)menuState.cursor_row - ((int8_t)LCD_N_ROWS - 1));
        }
        else if (menuState.cursor_row + 1 == menuState.n_items)
        {
            // Wrap around to top
            menuState.cursor_row = 0;
            menuState.menu_top_row = 0;
        }
    }
    else if (customKey == 'A')
    { // Up

        if (menuState.cursor_row > 0)
        {
            menuState.cursor_row--;
            if (menuState.menu_top_row > menuState.cursor_row)
                menuState.menu_top_row = menuState.cursor_row;
        }
        else
        {
            // Wrap around to bottom
            menuState.cursor_row = menuState.n_items - 1;
            // if (menuState.menu_top_row + LCD_N_ROWS - 1 < menuState.cursor_row)
            menuState.menu_top_row = max(0, (int8_t)menuState.cursor_row - ((int8_t)LCD_N_ROWS - 1));
        }
    }
    else if (customKey == '*')
    { // Select

        menu_select(menuState.cursor_row);
        // Skip updating the LCD
        return;
    }
    else
    {

        // Skip updating the LCD
        return;
    }
    menu_draw();
}

void menu_draw()
{
    for (uint8_t row = 0; row < LCD_N_ROWS; ++row)
    {
        lcd.setCursor(0, row);
        uint8_t idx = row + menuState.menu_top_row;
        lcd.print((idx == menuState.cursor_row) ? '>' : ' ');
        uint8_t n;
        if (idx < menuState.n_items)
        {
            n = strlen_P(pgm_read_ptr(menuState.labels + idx));
            lcd.print(toFSH((char const *)pgm_read_ptr(menuState.labels + idx)));
        }
        else
        {
            n = 0;
        }
        // Clear end of line
        for (uint8_t i = n; i < LCD_N_COLS - 1; ++i)
            lcd.print(' ');
    }
}

void menu_select(uint8_t menu_idx)
{
    if (menu_idx == 0 || menu_idx == 1)
    {
        // Set on/off time
        number_entry_init(menu_idx);
    }
    else
    {
        // Back to monitor screen
        monitor_init();
    }
}

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

bool number_entry_check()
{
    char c = g_number_entry_string[g_number_entry_column];
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
        monitor_init();
    }
}

static __FlashStringHelper const *toFSH(char const *progmem_ptr)
{
    return reinterpret_cast<__FlashStringHelper const *>(progmem_ptr);
}
