#include "bluetooth_component.h"

#include <stdbool.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "store/config/ble_store_config.h"

extern void ble_store_config_init(void);

static const char *TAG = "bluetooth";

static bluetooth_rx_callback_t s_rx_callback;
static void *s_rx_callback_ctx;
static bool s_initialized;
static uint8_t s_own_addr_type;

static const ble_uuid128_t g_ble_service_uuid = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);

static const ble_uuid128_t g_ble_rx_char_uuid = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);

static void bluetooth_advertise(void);

static esp_err_t bluetooth_status_gpio_init(void)
{
    const gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << BLE_STATUS_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "Failed to configure BLE status GPIO");
    ESP_RETURN_ON_ERROR(gpio_set_level(BLE_STATUS_GPIO, 0), TAG, "Failed to reset BLE status GPIO");
    return ESP_OK;
}

static int ble_rx_characteristic_access(uint16_t conn_handle,
                                        uint16_t attr_handle,
                                        struct ble_gatt_access_ctxt *ctxt,
                                        void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    uint16_t rx_len = OS_MBUF_PKTLEN(ctxt->om);
    if (rx_len == 0) {
        return 0;
    }

    if (rx_len > BLE_RX_MAX_LEN) {
        ESP_LOGW(TAG, "Incoming BLE payload truncated from %u to %u bytes", rx_len, BLE_RX_MAX_LEN);
        rx_len = BLE_RX_MAX_LEN;
    }

    uint8_t buffer[BLE_RX_MAX_LEN] = {0};
    const int rc = os_mbuf_copydata(ctxt->om, 0, rx_len, buffer);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to copy BLE payload from mbuf: %d", rc);
        return BLE_ATT_ERR_UNLIKELY;
    }

    ESP_LOGI(TAG, "BLE RX: %u bytes received from iPad/app", rx_len);
    ESP_LOG_BUFFER_HEXDUMP(TAG, buffer, rx_len, ESP_LOG_INFO);

    if (s_rx_callback != NULL) {
        s_rx_callback(buffer, rx_len, s_rx_callback_ctx);
    }

    return 0;
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &g_ble_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &g_ble_rx_char_uuid.u,
                .access_cb = ble_rx_characteristic_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {0},
        },
    },
    {0},
};

static int ble_gap_event_handler(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "BLE central connected");
            gpio_set_level(BLE_STATUS_GPIO, 1);
        } else {
            ESP_LOGW(TAG, "BLE connection failed: %d", event->connect.status);
            bluetooth_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE central disconnected, reason=%d", event->disconnect.reason);
        gpio_set_level(BLE_STATUS_GPIO, 0);
        bluetooth_advertise();
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "BLE advertising completed, restarting");
        bluetooth_advertise();
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "BLE MTU updated: mtu=%d", event->mtu.value);
        return 0;

    default:
        return 0;
    }
}

static void bluetooth_advertise(void)
{
    struct ble_hs_adv_fields fields = {0};
    const char *device_name = ble_svc_gap_device_name();

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (const uint8_t *)device_name;
    fields.name_len = (uint8_t)strlen(device_name);
    fields.name_is_complete = 1;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields failed: %d", rc);
        return;
    }

    struct ble_gap_adv_params adv_params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
        .channel_map = 0x07, // all channels
        .itvl_min = 0x00A0, // 20 ms
        .itvl_max = 0x00A0, // 40 ms
    };

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start failed: %d", rc);
    } else {
        ESP_LOGI(TAG, "Advertising BLE service '%s'", BLE_DEVICE_NAME);
    }
}

static void bluetooth_on_sync(void)
{
    const int rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_id_infer_auto failed: %d", rc);
        return;
    }

    bluetooth_advertise();
}

static void bluetooth_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void bluetooth_component_register_rx_callback(bluetooth_rx_callback_t callback, void *ctx)
{
    s_rx_callback = callback;
    s_rx_callback_ctx = ctx;
}

esp_err_t bluetooth_component_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(bluetooth_status_gpio_init(), TAG, "BLE GPIO init failed");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "NVS erase failed");
        err = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(err, TAG, "NVS init failed");

    err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(err));
        return err;
    }

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_svc_gap_device_name_set(BLE_DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_svc_gap_device_name_set failed: %d", rc);
        err = ESP_FAIL;
        goto fail;
    }

    rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        err = ESP_FAIL;
        goto fail;
    }

    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
        err = ESP_FAIL;
        goto fail;
    }

    ble_hs_cfg.sync_cb = bluetooth_on_sync;
    ble_store_config_init();
    nimble_port_freertos_init(bluetooth_host_task);

    s_initialized = true;
    ESP_LOGI(TAG, "Bluetooth component initialized");
    return ESP_OK;

fail:
    (void)nimble_port_deinit();
    (void)gpio_set_level(BLE_STATUS_GPIO, 0);
    return err;
}

esp_err_t bluetooth_component_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    int rc = nimble_port_stop();
    if (rc != 0) {
        ESP_LOGW(TAG, "nimble_port_stop returned: %d", rc);
    }

    esp_err_t err = nimble_port_deinit();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nimble_port_deinit returned: %s", esp_err_to_name(err));
    }

    ESP_RETURN_ON_ERROR(gpio_set_level(BLE_STATUS_GPIO, 0), TAG, "Failed to clear BLE status GPIO");

    s_initialized = false;
    ESP_LOGI(TAG, "Bluetooth component deinitialized");
    return ESP_OK;
}
