#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool http_server_start(void);
void http_server_stop(void);

#ifdef __cplusplus
}
#endif
