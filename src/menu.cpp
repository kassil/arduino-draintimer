#include "menu.h"
#include "drain_timer.h"
#include "monitor.h"
#include "number_entry.h"
#include <Keypad_I2C.h>
#include <LiquidCrystal_I2C.h>

struct MenuState
{
    uint8_t cursor_row;
    uint8_t menu_top_row;
    uint8_t n_items;
    char const *const *labels;
};
MenuState menuState;

static __FlashStringHelper const *toFSH(char const *progmem_ptr);

void menu_enter(uint8_t n_items, char const *const *labels)
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
            n = strlen_P((char const*)pgm_read_ptr(menuState.labels + idx));
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
        monitor_enter();
    }
}

static __FlashStringHelper const *toFSH(char const *progmem_ptr)
{
    return reinterpret_cast<__FlashStringHelper const *>(progmem_ptr);
}
