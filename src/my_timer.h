#pragma once
#include <stdint.h>

class Print;

void print_hms_time(Print& target, unsigned long milliseconds);

extern bool g_update_state;

extern unsigned long switch_millis;
extern uint8_t g_switch_state;

// Call this once at boot-up
void timer_init();

// Call this frequently to service the timer
void timer_loop();

// Get the duration of the active timer (not time remaining)
unsigned long timer_get_duration();

// Get the duration of the desired timer (not time remaining)
unsigned long timer_get_duration(uint8_t idx);

// Set the total time of the desired timer
void timer_set_duration(uint8_t idx, unsigned long duration_ms);
