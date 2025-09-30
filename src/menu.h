#pragma once
#include <stdint.h>

// Call this to begin showing the menu
void menu_enter(uint8_t n_items, char const *const *labels);

// Call this periodically to service the menu
void menu_loop();
