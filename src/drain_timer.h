/*
 * Here are the global variables and routines for the project.
 */
#pragma once
#include <stdint.h>

class Keypad_I2C;
class LiquidCrystal_I2C;

constexpr uint8_t LCD_N_ROWS = 4;
constexpr uint8_t LCD_N_COLS = 20;

extern Keypad_I2C customKeypad;
extern LiquidCrystal_I2C lcd;

// What function we call in our loop.  This changes with the state.
extern void (*loop_function)();

void mainmenu_enter();
