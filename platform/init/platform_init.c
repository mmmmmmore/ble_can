#include "platform_init.h"

#include "bluetooth_component.h"
#include "bluetooth_rx.h"
#include "esp_log.h"
#include "twai_component.h"
#include "twai_tx.h"

static const char *TAG = "platform_init";
static platform_state_t s_state = PLATFORM_STATE_STOPPED;

platform_state_t platform_init_get_state(void)
{
    return s_state;
}

esp_err_t platform_init_startup(void)
{
    if (s_state == PLATFORM_STATE_RUNNING) {
        ESP_LOGI(TAG, "Platform already running");
        return ESP_OK;
    }

    if (s_state != PLATFORM_STATE_STOPPED) {
        ESP_LOGW(TAG, "Platform busy, current state=%d", s_state);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err;
    s_state = PLATFORM_STATE_STARTING;

    err = twai_component_init();
    if (err != ESP_OK) {
        goto fail;
    }

    err = twai_tx_init();
    if (err != ESP_OK) {
        goto fail;
    }

    err = bluetooth_rx_init();
    if (err != ESP_OK) {
        goto fail;
    }

    err = bluetooth_component_init();
    if (err != ESP_OK) {
        goto fail;
    }

    s_state = PLATFORM_STATE_RUNNING;
    ESP_LOGI(TAG, "Platform startup complete");
    return ESP_OK;

fail:
    ESP_LOGE(TAG, "Platform startup failed: %s", esp_err_to_name(err));
    (void)bluetooth_component_deinit();
    (void)bluetooth_rx_deinit();
    (void)twai_tx_deinit();
    (void)twai_component_deinit();
    s_state = PLATFORM_STATE_STOPPED;
    return err;
}

esp_err_t platform_init_shutdown(void)
{
    if (s_state == PLATFORM_STATE_STOPPED) {
        return ESP_OK;
    }

    s_state = PLATFORM_STATE_STOPPING;

    (void)bluetooth_component_deinit();
    (void)bluetooth_rx_deinit();
    (void)twai_tx_deinit();
    (void)twai_component_deinit();

    s_state = PLATFORM_STATE_STOPPED;
    ESP_LOGI(TAG, "Platform shutdown complete");
    return ESP_OK;
}
