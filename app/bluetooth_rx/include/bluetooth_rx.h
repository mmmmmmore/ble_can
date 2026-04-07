#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bluetooth_rx_init(void);
esp_err_t bluetooth_rx_deinit(void);

#ifdef __cplusplus
}
#endif
