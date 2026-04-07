#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t twai_tx_init(void);
esp_err_t twai_tx_deinit(void);
esp_err_t twai_tx_forward_bytes(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif
