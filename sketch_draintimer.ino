/*
 Demonstrates the use of a 16x2 LCD display.
 
 Demonstrates polling debounced buttons

 Demonstrates a menu
*/

#define LCD_I2C

// include the library code:
#include <Arduino.h>
#include <Keypad_I2C.h>
#include <Keypad.h>        // GDY120705
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
//define the cymbols on the buttons of the keypads
char hexaKeys[KPD_ROWS][KPD_COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[KPD_ROWS] = {0, 1, 2, 3}; //connect to the row pinouts of the keypad
byte colPins[KPD_COLS] = {4, 5, 6, 7}; //connect to the column pinouts of the keypad
Keypad_I2C customKeypad( makeKeymap(hexaKeys), rowPins, colPins, KPD_ROWS, KPD_COLS, KPD_SLAVE); 

constexpr uint8_t LCD_N_ROWS = 4;
constexpr uint8_t LCD_N_COLS = 20;
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
void menu_init();
void menu_loop();
static __FlashStringHelper const* toFSH(char const* progmem_ptr);

unsigned long last_time = 0;

void (*loop_function)();

uint8_t cursor_row = 0, menu_top_row = 0;
struct MenuItems
{
    char const PROGMEM labels[N_MENU_ITEMS][8] PROGMEM;
    byte N_MENU_ITEMS = 9;
};

constexpr size_t N_MENU_ITEMS = 9;
char const PROGMEM menu[N_MENU_ITEMS][8] PROGMEM = {
    "Matthew",
    "Mark",
    "Luke",
    "John",
    "Peter",
    "Paul",
    "John",
    "George",
    "Ringo",
};

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);           // Turn the LED on.

    Wire.begin( );                // GDY200622

    Serial.begin(9600);
    while( !Serial ){ /*wait*/ }

    // set up the LCD's number of columns and rows:
#ifdef LCD_I2C
    lcd.init();
    lcd.backlight();
#else
    lcd.begin(LCD_N_COLS, LCD_N_ROWS);
#endif

    customKeypad.begin( );        // GDY120705

    monitor_init(); 

    Serial.println(F("Hello, world!"));
    digitalWrite(LED_BUILTIN, LOW);           // Turn the LED off.
}

void loop()
{
    loop_function();

    unsigned long now = millis();
    if (now - last_time < 10)
    {
        delay(now - last_time);
    }
    last_time = now;
}

void monitor_init()
{
    loop_function = monitor_loop;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("\xDF Menu   ^ LED"));
    lcd.setCursor(0, 1);
    lcd.print(F("LED:"));
    lcd.setCursor(0, 2);
    lcd.print(F("Selection:"));
    lcd.setCursor(0, 3);
    lcd.print(toFSH(menu[cursor_row]));

    Serial.print(F("Monitor init strlen "));
    Serial.println(strlen_P(menu[cursor_row]));

    //         12345678901234567890
    // Clear end of line
    for (uint8_t i = strlen_P(menu[cursor_row]); i < LCD_N_COLS - 1; ++i)
        lcd.print(' ');
}

void monitor_loop()
{
    char const customKey = customKeypad.getKey();
    if (customKey != NO_KEY){
        Serial.println(customKey);
    }

    if (customKey == '*') {
        menu_init();
    }
    else if (customKey == 'A') {
        // Toggle LED
        uint8_t ledState = !digitalRead(LED_BUILTIN);
        digitalWrite(LED_BUILTIN, ledState);
        lcd.setCursor(17,1);
        lcd.print(ledState ? F("On ") : F("Off"));
    }
}

void menu_init()
{
    //cursor_row = 0;
    //menu_top_row = 0;
    loop_function = menu_loop;
}

void menu_loop()
{
    for (uint8_t row = 0; row < LCD_N_ROWS; ++row)
    {
        lcd.setCursor(0, row);
        uint8_t menu_row = row + menu_top_row;
        lcd.print((menu_row == cursor_row) ? '>' : ' ');
        uint8_t n;
        if (menu_row < N_MENU_ITEMS) {
            lcd.print(toFSH(menu[menu_row]));
            n = strlen_P(menu[menu_row]);
        }
        else {
            n = 0;
        }
        // Clear end of line
        for (uint8_t i = n; i < LCD_N_COLS - 1; ++i)
            lcd.print(' ');
    }

    char const customKey = customKeypad.getKey();
    if (customKey != NO_KEY){
        Serial.println(customKey);
    }

    if (customKey == 'D') { // Down

        if (cursor_row + 1u < N_MENU_ITEMS)
        {
            cursor_row++;
            if (menu_top_row + LCD_N_ROWS - 1 < cursor_row)
                menu_top_row = max(0, (int8_t) cursor_row - ((int8_t) LCD_N_ROWS - 1));
        }
        else if (cursor_row + 1 == N_MENU_ITEMS)
        {
            // Wrap around to top
            cursor_row = 0;
            menu_top_row = 0;
        }
    }
    else if (customKey == 'A') {  // Up
        if (cursor_row > 0)
        {
            cursor_row--;
            if (menu_top_row > cursor_row)
                menu_top_row = cursor_row;
        }
        else
        {
            // Wrap around to bottom
            cursor_row = N_MENU_ITEMS - 1;
            //if (menu_top_row + LCD_N_ROWS - 1 < cursor_row)
            menu_top_row = max(0, (int8_t) cursor_row - ((int8_t) LCD_N_ROWS - 1));
        }
    }
    else if (customKey == '*') { // Select
        monitor_init();
    }
}

static __FlashStringHelper const* toFSH(char const* progmem_ptr)
{
    return reinterpret_cast<__FlashStringHelper const*>(progmem_ptr);
}
