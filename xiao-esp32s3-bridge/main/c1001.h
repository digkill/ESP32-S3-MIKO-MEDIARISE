#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool c1001_init(void);
bool c1001_is_ok(void);

// Snapshot query — blocks up to ~200 ms.
// presence: 0=absent 1=present
// motion:   0=none 1=still 2=active
// range_cm: movement magnitude in cm
bool c1001_query(uint8_t* presence, uint8_t* motion, uint16_t* range_cm);

#ifdef __cplusplus
}
#endif
