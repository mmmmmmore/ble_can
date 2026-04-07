#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "platform_init.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32-S3 BLE -> TWAI transparent bridge");

    const esp_err_t err = platform_init_startup();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "platform_init_startup failed: %s", esp_err_to_name(err));
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
