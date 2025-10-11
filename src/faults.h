#pragma once
#include <stdint.h>

void diags_enter();

void faultmenu_enter();

// Return true if fault menu is currently active (so callers can skip sampling)
bool faultmenu_is_active();
