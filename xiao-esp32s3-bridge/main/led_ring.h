#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool led_ring_init(void);
bool led_ring_is_ok(void);
void led_ring_set_all(uint8_t r, uint8_t g, uint8_t b);
void led_ring_set_pixel(int idx, uint8_t r, uint8_t g, uint8_t b);
void led_ring_clear(void);
void led_ring_set_default(void);
void led_ring_test(void);

// Бегущая радуга по кольцу (вызывать из main loop).
void led_ring_service(void);
void led_ring_rainbow_enable(bool on);
bool led_ring_rainbow_active(void);

#ifdef __cplusplus
}
#endif
