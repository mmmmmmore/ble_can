#include "can.h"

#include <stdbool.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "CAN_DRIVER";
static bool is_initialized = false;

#define CAN_RECOVERY_TIMEOUT_MS 1000U

static void can_log_status(const char *context)
{
    twai_status_info_t status = {0};
    if (twai_get_status_info(&status) != ESP_OK) {
        return;
    }

    ESP_LOGW(TAG,
             "%s: state=%d tx_err=%lu rx_err=%lu msgs_to_tx=%lu msgs_to_rx=%lu tx_failed=%lu arb_lost=%lu bus_error=%lu",
             context,
             status.state,
             (unsigned long)status.tx_error_counter,
             (unsigned long)status.rx_error_counter,
             (unsigned long)status.msgs_to_tx,
             (unsigned long)status.msgs_to_rx,
             (unsigned long)status.tx_failed_count,
             (unsigned long)status.arb_lost_count,
             (unsigned long)status.bus_error_count);
}

static esp_err_t can_try_recover(void)
{
    twai_status_info_t status = {0};
    if (twai_get_status_info(&status) != ESP_OK) {
        return ESP_FAIL;
    }

    if (status.state != TWAI_STATE_BUS_OFF) {
        return ESP_OK;
    }

    esp_err_t recovery_err = twai_initiate_recovery();
    ESP_LOGW(TAG, "TWAI bus-off detected, initiating recovery: %s", esp_err_to_name(recovery_err));
    if (recovery_err != ESP_OK && recovery_err != ESP_ERR_INVALID_STATE) {
        return recovery_err;
    }

    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(CAN_RECOVERY_TIMEOUT_MS);
    while (xTaskGetTickCount() <= deadline) {
        uint32_t alerts = 0;
        TickType_t remaining = deadline - xTaskGetTickCount();
        esp_err_t alert_err = twai_read_alerts(&alerts, remaining);
        if (alert_err == ESP_ERR_TIMEOUT) {
            break;
        }
        if (alert_err != ESP_OK) {
            return alert_err;
        }
        if ((alerts & TWAI_ALERT_BUS_RECOVERED) != 0) {
            ESP_LOGI(TAG, "TWAI bus recovered");
            esp_err_t start_err = twai_start();
            if (start_err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to restart TWAI after recovery: %s", esp_err_to_name(start_err));
                return start_err;
            }
            ESP_LOGI(TAG, "TWAI restarted after recovery");
            return ESP_OK;
        }
    }

    ESP_LOGW(TAG, "Timed out waiting for TWAI recovery");
    return ESP_ERR_TIMEOUT;
}

bool can_driver_is_initialized(void)
{
    return is_initialized;
}

esp_err_t can_driver_init(void)
{
    if (is_initialized) {
        ESP_LOGW(TAG, "CAN driver already initialized");
        return ESP_OK;
    }

    twai_timing_config_t t_config = CAN_BITRATE;
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_GPIO_NUM, CAN_RX_GPIO_NUM, CAN_TWAI_MODE);
    g_config.tx_queue_len = 10;
    g_config.rx_queue_len = 20;
    g_config.alerts_enabled = TWAI_ALERT_TX_SUCCESS |
                              TWAI_ALERT_TX_FAILED |
                              TWAI_ALERT_BUS_OFF |
                              TWAI_ALERT_RECOVERY_IN_PROGRESS |
                              TWAI_ALERT_BUS_RECOVERED |
                              TWAI_ALERT_ERR_PASS;

    esp_err_t ret = twai_driver_install(&g_config, &t_config, &f_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install TWAI driver: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = twai_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start TWAI driver: %s", esp_err_to_name(ret));
        (void)twai_driver_uninstall();
        return ret;
    }

    is_initialized = true;
    ESP_LOGI(TAG,
             "CAN driver initialized (TX=GPIO%d RX=GPIO%d bitrate=500k mode=%d)",
             CAN_TX_GPIO_NUM,
             CAN_RX_GPIO_NUM,
             CAN_TWAI_MODE);
    return ESP_OK;
}

esp_err_t can_send_message(uint32_t id, const uint8_t *data, uint8_t len, uint32_t timeout_ms)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "CAN driver not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (len > 8) {
        ESP_LOGE(TAG, "Data length exceeds 8 bytes");
        return ESP_ERR_INVALID_ARG;
    }

    twai_message_t message = {
        .identifier = id,
        .data_length_code = len,
        .extd = 0,
        .rtr = 0,
        .ss = 0,
        .self = 0,
        .dlc_non_comp = 0,
    };

    if (data != NULL && len > 0) {
        memcpy(message.data, data, len);
    }

    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);

    for (int attempt = 0; attempt < 2; ++attempt) {
        esp_err_t ret = twai_transmit(&message, ticks);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG,
                     "CAN TX queued: ID=0x%03lX Len=%u%s",
                     (unsigned long)id,
                     (unsigned int)len,
                     (attempt == 0) ? "" : " (after recovery)");
            return ESP_OK;
        }

        can_log_status("twai_transmit failed");
        if (ret == ESP_ERR_INVALID_STATE) {
            esp_err_t recovery_err = can_try_recover();
            if (recovery_err == ESP_OK && attempt == 0) {
                ESP_LOGW(TAG, "Retrying CAN frame after recovery");
                continue;
            }
        }

        ESP_LOGE(TAG, "Failed to send CAN message: %s (ID=0x%03lX Len=%u)",
                 esp_err_to_name(ret),
                 (unsigned long)id,
                 (unsigned int)len);
        return ret;
    }

    return ESP_FAIL;
}

esp_err_t can_receive_message(uint32_t *id, uint8_t *data, uint8_t *len, uint32_t timeout_ms)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "CAN driver not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    twai_message_t message = {0};
    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);

    esp_err_t ret = twai_receive(&message, ticks);
    if (ret != ESP_OK) {
        if (ret != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "Failed to receive CAN message: %s", esp_err_to_name(ret));
        }
        return ret;
    }

    if (id != NULL) {
        *id = message.identifier;
    }
    if (len != NULL) {
        *len = message.data_length_code;
    }
    if (data != NULL && message.data_length_code > 0) {
        memcpy(data, message.data, message.data_length_code);
    }

    ESP_LOGD(TAG, "CAN RX: ID=0x%03lX Len=%u",
             (unsigned long)message.identifier,
             (unsigned int)message.data_length_code);
    return ESP_OK;
}

esp_err_t can_driver_deinit(void)
{
    if (!is_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = twai_stop();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to stop TWAI driver: %s", esp_err_to_name(ret));
    }

    ret = twai_driver_uninstall();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to uninstall TWAI driver: %s", esp_err_to_name(ret));
        return ret;
    }

    is_initialized = false;
    ESP_LOGI(TAG, "CAN driver deinitialized");
    return ESP_OK;
}
