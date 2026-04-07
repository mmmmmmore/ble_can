#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PLATFORM_STATE_STOPPED = 0,
    PLATFORM_STATE_STARTING,
    PLATFORM_STATE_RUNNING,
    PLATFORM_STATE_STOPPING,
} platform_state_t;

esp_err_t platform_init_startup(void);
esp_err_t platform_init_shutdown(void);
platform_state_t platform_init_get_state(void);

#ifdef __cplusplus
}
#endif
