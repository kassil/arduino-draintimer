#pragma once
#include <stdint.h>

void menu_enter(uint8_t n_items, char const *const *labels);
void menu_loop();
void menu_draw();
void menu_select(uint8_t menu_idx);
