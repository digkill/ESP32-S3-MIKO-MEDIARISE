#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool servo_init(void);
bool servo_is_ok(void);

// Queue a smooth move toward (yaw, pitch). Clamps to [SERVO_MIN_ANGLE..SERVO_MAX_ANGLE].
void servo_set_target(int yaw, int pitch);

// Returns current physical positions.
int servo_get_yaw(void);
int servo_get_pitch(void);

// Returns the clamped targets accepted by the last servo_set_target().
int servo_get_target_yaw(void);
int servo_get_target_pitch(void);

// Call from main loop / timer to advance one step toward target.
void servo_service(void);

// Power management: detach PWM to stop jitter, reattach on next move.
void servo_detach(void);
bool servo_reattach(void);
bool servo_is_attached(void);
bool servo_is_moving(void);

#ifdef __cplusplus
}
#endif
