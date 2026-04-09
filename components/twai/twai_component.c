#include "twai_component.h"

#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "twai_compat";

esp_err_t twai_component_init(void)
{
    return can_driver_init();
}

esp_err_t twai_component_deinit(void)
{
    return can_driver_deinit();
}

esp_err_t twai_component_transmit(uint32_t identifier, const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(data != NULL || len == 0, ESP_ERR_INVALID_ARG, TAG, "Payload pointer is invalid");
    ESP_RETURN_ON_FALSE(len <= 8U, ESP_ERR_INVALID_SIZE, TAG, "TWAI frame length must be <= 8 bytes");

    return can_send_message(identifier, data, (uint8_t)len, timeout_ms);
}
