#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t  version;
    uint8_t  type;
    uint16_t sequence;
    char     payload[ESPNOW_PAYLOAD_SIZE];
} espnow_packet_t;

typedef struct {
    uint8_t sender[6];
    uint16_t sequence;
    char command[ESPNOW_PAYLOAD_SIZE];
} espnow_queued_cmd_t;

bool espnow_init(void);
bool espnow_is_ok(void);

// Service pending commands — call frequently from main task or dedicated task.
void espnow_service(void);

// Start dedicated FreeRTOS task that runs espnow_service() in a loop.
bool espnow_start_task(void);

#ifdef __cplusplus
}
#endif
