#include "twai_tx.h"

#include <stdbool.h>

#include "can.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "twai_tx";
static bool s_initialized;

esp_err_t twai_tx_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(can_driver_init(), TAG, "CAN driver init failed");

    s_initialized = true;
    ESP_LOGI(TAG,
             "BLE -> CAN bridge ready (TX=0x%03lX RX=0x%03lX)",
             (unsigned long)CAN_DEFAULT_TX_ID,
             (unsigned long)CAN_DEFAULT_RX_ID);
    return ESP_OK;
}

esp_err_t twai_tx_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(can_driver_deinit(), TAG, "CAN driver deinit failed");

    s_initialized = false;
    ESP_LOGI(TAG, "BLE -> CAN bridge stopped");
    return ESP_OK;
}

esp_err_t twai_tx_forward_bytes(const uint8_t *data, size_t len)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "BLE -> CAN bridge is not initialized");
    ESP_RETURN_ON_FALSE(data != NULL || len == 0, ESP_ERR_INVALID_ARG, TAG, "Payload pointer is invalid");
    ESP_RETURN_ON_FALSE(len > 0, ESP_ERR_INVALID_SIZE, TAG, "BLE payload is empty");

    size_t offset = 0;
    uint32_t frame_index = 0;

    while (offset < len) {
        const uint8_t chunk_len = (uint8_t)(((len - offset) > 8U) ? 8U : (len - offset));
        esp_err_t err = can_send_message(CAN_DEFAULT_TX_ID, &data[offset], chunk_len, 100);
        if (err != ESP_OK) {
            ESP_LOGE(TAG,
                     "CAN transmit failed at chunk %lu: %s",
                     (unsigned long)frame_index,
                     esp_err_to_name(err));
            return err;
        }

        ESP_LOGI(TAG,
                 "Forwarded BLE chunk %lu as CAN frame (ID=0x%03lX Len=%u)",
                 (unsigned long)frame_index,
                 (unsigned long)CAN_DEFAULT_TX_ID,
                 (unsigned int)chunk_len);

        offset += chunk_len;
        frame_index++;
    }

    return ESP_OK;
}
