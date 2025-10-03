/*
 * Here are the global variables and routines for the project.
 */
#pragma once
#include <stdint.h>

class Keypad_I2C;
class LiquidCrystal_I2C;
class PCF8574;

constexpr uint8_t LCD_N_ROWS = 4;
constexpr uint8_t LCD_N_COLS = 20;

extern Keypad_I2C customKeypad;
extern LiquidCrystal_I2C lcd;
extern PCF8574 relays;

enum Relays {
    FillSolenoid,
    Dispenser,
    WashMotor,
    DrainMotor,
    Pilot,
    HeaterL,
    HeaterN,
};

// What function we call in our loop.  This changes with the state.
extern void (*loop_function)();

// Enter the main menu
void mainmenu_enter();
