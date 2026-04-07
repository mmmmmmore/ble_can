#pragma once

#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_STATUS_GPIO GPIO_NUM_2
#define BLE_DEVICE_NAME "ESP32S3-BLE-CAN"
#define BLE_RX_MAX_LEN 244

typedef void (*bluetooth_rx_callback_t)(const uint8_t *data, size_t len, void *ctx);

esp_err_t bluetooth_component_init(void);
esp_err_t bluetooth_component_deinit(void);
void bluetooth_component_register_rx_callback(bluetooth_rx_callback_t callback, void *ctx);

#ifdef __cplusplus
}
#endif
