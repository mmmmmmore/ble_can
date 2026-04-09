#include "bluetooth_rx.h"

#include "bluetooth_component.h"
#include "esp_log.h"
#include "twai_tx.h"

static const char *TAG = "bluetooth_rx";

static void bluetooth_rx_forwarder(const uint8_t *data, size_t len, void *ctx)
{
    (void)ctx;

    esp_err_t err = twai_tx_forward_bytes(data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to forward BLE payload to CAN: %s", esp_err_to_name(err));
    }
}

esp_err_t bluetooth_rx_init(void)
{
    bluetooth_component_register_rx_callback(bluetooth_rx_forwarder, NULL);
    ESP_LOGI(TAG, "Bluetooth RX bridge ready");
    return ESP_OK;
}

esp_err_t bluetooth_rx_deinit(void)
{
    bluetooth_component_register_rx_callback(NULL, NULL);
    ESP_LOGI(TAG, "Bluetooth RX bridge stopped");
    return ESP_OK;
}
