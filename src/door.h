#pragma once
#include <stdint.h>

// Sample the door input; call this regularly from the main loop (or from a
// timer). Debouncing is performed inside this module.
void door_tick();

// Returns true if door is currently considered open (debounced state).
bool door_is_open();


void pilot_init();
void pilot_off();
void pilot_on();
