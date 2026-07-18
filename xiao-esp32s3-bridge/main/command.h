#pragma once
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Execute a text command. Result is written as a newline-terminated string
// into out_buf (max out_size bytes, including null terminator).
void command_execute(const char* cmd, char* out_buf, size_t out_size);

// Returns true while the device is in power-save mode.
bool command_power_save_active(void);

// Call from main loop: enters power-save if XIAO_POWER_SAVE_TIMEOUT_MS elapsed.
void command_check_idle_timeout(void);

#ifdef __cplusplus
}
#endif
