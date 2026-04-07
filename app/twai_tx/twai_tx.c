#include "twai_tx.h"

#include "esp_check.h"
#include "esp_log.h"
#include "twai_component.h"

static const char *TAG = "twai_tx";

esp_err_t twai_tx_init(void)
{
    ESP_LOGI(TAG, "TWAI TX bridge ready, default CAN ID=0x%03X", TWAI_DEFAULT_CAN_ID);
    return ESP_OK;
}

esp_err_t twai_tx_deinit(void)
{
    ESP_LOGI(TAG, "TWAI TX bridge stopped");
    return ESP_OK;
}

esp_err_t twai_tx_forward_bytes(const uint8_t *data, size_t len)
{
    ESP_RETURN_ON_FALSE(data != NULL || len == 0, ESP_ERR_INVALID_ARG, TAG, "Payload pointer is invalid");

    size_t offset = 0;
    while (offset < len) {
        const size_t chunk_len = ((len - offset) > 8U) ? 8U : (len - offset);
        esp_err_t err = twai_component_transmit(TWAI_DEFAULT_CAN_ID, &data[offset], chunk_len, 100);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "TWAI transmit failed at offset %u: %s", (unsigned int)offset, esp_err_to_name(err));
            return err;
        }
        offset += chunk_len;
    }

    ESP_LOGI(TAG, "Forwarded %u BLE bytes as TWAI frames", (unsigned int)len);
    return ESP_OK;
}
