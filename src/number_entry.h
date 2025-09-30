#pragma once

// Enter the number entry mode
void number_entry_init(int target_idx);

// Periodically call this to service the number entry mode.
void number_entry_loop();
