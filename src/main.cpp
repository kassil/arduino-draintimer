#define LCD_I2C

#include "drain_timer.h"
#include "diags.h"
#include "menu.h"
#include "monitor.h"
#include "wash.h"

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

static void mainmenu_select(uint8_t menu_idx);

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


// What function we call in our loop.  This changes with the state.
void (*loop_function)();

constexpr byte mainmenu_n_items = 7;
const char mainmenu_labels_0[] PROGMEM = "Wash";
const char mainmenu_labels_1[] PROGMEM = "Drain";
const char mainmenu_labels_2[] PROGMEM = "Rinse";
const char mainmenu_labels_3[] PROGMEM = "Delay Wash 1h";
const char mainmenu_labels_4[] PROGMEM = "Delay Wash 4h";
const char mainmenu_labels_5[] PROGMEM = "Delay Wash 8h";
const char mainmenu_labels_6[] PROGMEM = "Diagnostics";
const char *const mainmenu_labels[mainmenu_n_items] PROGMEM = {
    mainmenu_labels_0,
    mainmenu_labels_1,
    mainmenu_labels_2,
    mainmenu_labels_3,
    mainmenu_labels_4,
    mainmenu_labels_5,
    mainmenu_labels_6,
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

    mainmenu_enter();

    digitalWrite(LED_BUILTIN, LOW); // Turn the LED off.
}

void loop()
{
    loop_function();
}

void mainmenu_enter()
{
    menu_enter(mainmenu_n_items, mainmenu_labels, mainmenu_select);
}

void mainmenu_select(uint8_t menu_idx)
{
    Serial.print(F("mm sel "));
    Serial.println(menu_idx);
    if (menu_idx == 0)
    {
        // Wash
        delay_cycle_enter(0, 2, 1);
    }
    else if (menu_idx == 1)
    {
        // Drain
        delay_cycle_enter(0, 0, 0);
    }
    else if (menu_idx == 2)
    {
        // Rinse
        delay_cycle_enter(0, 0, 1);
    }
    //else Delay wash 1/4/8h
    else if (menu_idx == 3)
    {
        delay_cycle_enter(1ul*1000*10, 2, 1);
    }
    else if (menu_idx == 4)
    {
        delay_cycle_enter(4ul*1000*10, 2, 1);
    }
    else if (menu_idx == 5)
    {
        delay_cycle_enter(8ul*1000*10, 2, 1);
    }
    else if (menu_idx == 6)
    {
        diags_enter();
    }
    else
    {
        // Invalid menu selection
        Serial.print(F("mm inv "));
        Serial.println(menu_idx);
    }
}
