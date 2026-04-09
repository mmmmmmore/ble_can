#pragma once

#include <stddef.h>
#include <stdint.h>

#include "can.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Compatibility wrapper for older code paths that still include `twai_component.h`.
 * The canonical low-level TWAI/CAN transport now lives in `components/can`.
 */
#define TWAI_TX_GPIO        CAN_TX_GPIO_NUM
#define TWAI_RX_GPIO        CAN_RX_GPIO_NUM
#define TWAI_DEFAULT_CAN_ID CAN_DEFAULT_TX_ID

typedef struct {
    gpio_num_t tx_gpio;
    gpio_num_t rx_gpio;
    uint32_t default_identifier;
} twai_component_config_t;

esp_err_t twai_component_init(void);
esp_err_t twai_component_deinit(void);
esp_err_t twai_component_transmit(uint32_t identifier, const uint8_t *data, size_t len, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
