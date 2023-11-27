#pragma once
#include <stdint.h>

class Print;

void print_hms_time(Print& target, unsigned long milliseconds);

extern bool g_update_state;

extern unsigned long switch_millis;
extern uint8_t g_switch_state;

void timer_init();

void timer_loop();

unsigned long timer_get_duration();
