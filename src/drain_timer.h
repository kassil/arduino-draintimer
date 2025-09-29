#pragma once
#include <stdint.h>

class LiquidCrystal_I2C;

constexpr uint8_t LCD_N_ROWS = 4;
constexpr uint8_t LCD_N_COLS = 20;

extern LiquidCrystal_I2C lcd;
