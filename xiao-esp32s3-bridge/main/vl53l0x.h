#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool vl53_init(void);
bool vl53_is_ok(void);

// Returns true on success; mm_out = range in mm, status_out = range status (4 = out of range).
bool vl53_read(uint16_t* mm_out, uint8_t* status_out);

#ifdef __cplusplus
}
#endif
