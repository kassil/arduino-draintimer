#pragma once
#include <stdint.h>

// Sample the door input; call this regularly from the main loop (or from a
// timer). Debouncing is performed inside this module.
void door_tick();

// Returns true if door is currently considered open (debounced state).
bool door_is_open();
// Returns true if the door is currently debounced closed.
bool door_is_closed();

// Returns true once when the debounced state transitions from open->closed.
// The function returns true only once per transition (it clears the event).
bool door_was_closed();


void pilot_init();
void pilot_off();
void pilot_on();
