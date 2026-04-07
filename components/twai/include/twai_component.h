#pragma once

#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TWAI_TX_GPIO GPIO_NUM_5
#define TWAI_RX_GPIO GPIO_NUM_4
#define TWAI_DEFAULT_CAN_ID 0x321

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
