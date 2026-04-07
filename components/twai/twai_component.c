#include "twai_component.h"

#include <stdbool.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "twai";
static bool s_initialized;

static esp_err_t twai_gpio_init(void)
{
    const gpio_config_t tx_cfg = {
        .pin_bit_mask = 1ULL << TWAI_TX_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    const gpio_config_t rx_cfg = {
        .pin_bit_mask = 1ULL << TWAI_RX_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&tx_cfg), TAG, "TWAI TX GPIO config failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(TWAI_TX_GPIO, 1), TAG, "TWAI TX idle level set failed");
    ESP_RETURN_ON_ERROR(gpio_config(&rx_cfg), TAG, "TWAI RX GPIO config failed");
    return ESP_OK;
}

esp_err_t twai_component_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(twai_gpio_init(), TAG, "TWAI GPIO init failed");

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TWAI_TX_GPIO, TWAI_RX_GPIO, TWAI_MODE_NORMAL);
    g_config.tx_queue_len = 16;
    g_config.rx_queue_len = 16;
    g_config.alerts_enabled = TWAI_ALERT_TX_FAILED | TWAI_ALERT_BUS_OFF | TWAI_ALERT_ERR_PASS;

    const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_RETURN_ON_ERROR(twai_driver_install(&g_config, &t_config, &f_config), TAG, "TWAI driver install failed");
    ESP_RETURN_ON_ERROR(twai_start(), TAG, "TWAI start failed");

    s_initialized = true;
    ESP_LOGI(TAG, "TWAI initialized on TX=%d RX=%d at 500 kbit/s", TWAI_TX_GPIO, TWAI_RX_GPIO);
    return ESP_OK;
}

esp_err_t twai_component_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(twai_stop(), TAG, "TWAI stop failed");
    ESP_RETURN_ON_ERROR(twai_driver_uninstall(), TAG, "TWAI uninstall failed");

    s_initialized = false;
    ESP_LOGI(TAG, "TWAI component deinitialized");
    return ESP_OK;
}

esp_err_t twai_component_transmit(uint32_t identifier, const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "TWAI is not initialized");
    ESP_RETURN_ON_FALSE(data != NULL || len == 0, ESP_ERR_INVALID_ARG, TAG, "Payload pointer is invalid");
    ESP_RETURN_ON_FALSE(len <= 8, ESP_ERR_INVALID_SIZE, TAG, "TWAI frame length must be <= 8 bytes");

    twai_message_t message = {
        .identifier = identifier,
        .extd = 0,
        .rtr = 0,
        .data_length_code = len,
    };

    if (len > 0) {
        memcpy(message.data, data, len);
    }

    return twai_transmit(&message, pdMS_TO_TICKS(timeout_ms));
}
