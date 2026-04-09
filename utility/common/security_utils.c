#include "utility_common.h"
#include "esp_log.h"
#include "uds_protocol.h"

static const char *TAG = "UTILITY_SECURITY";

#define SEED_KEY_BYTES 4
#define LFSR_ITERATIONS 35

static uint32_t select_mask_by_security_level(uint8_t level)
{
    switch (level) {
        case 0x01:
            return 0x20454353U;
        case 0x02:
            return 0x20454353U;
        case 0x35:
            return 0x35454353U;
        case 0x36:
            return 0x35454353U;
        case 0x69:
            return 0x91E67F4AU;
        case 0x6A:
            return 0x91E67F4AU;
        default:
            return 0U;
    }
}

esp_err_t utility_security_generate_key(const uint8_t *seed, uint16_t seed_len,
                                        uint8_t *key, uint16_t *key_len,
                                        uint8_t level)
{
    if (seed == NULL || key == NULL || key_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (seed_len != SEED_KEY_BYTES) {
        ESP_LOGE(TAG, "Unsupported seed length (%u)", (unsigned)seed_len);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t session = uds_get_current_session();
    uint32_t mask = select_mask_by_security_level(level);
    if (mask == 0U) {
        ESP_LOGE(TAG, "No security mask available for level 0x%02X (session=0x%02X)",
             level, session);
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t lfsr = ((uint32_t)seed[0] << 24) |
                    ((uint32_t)seed[1] << 16) |
                    ((uint32_t)seed[2] << 8)  |
                    ((uint32_t)seed[3]);

    for (int i = 0; i < LFSR_ITERATIONS; ++i) {
        bool msb_set = (lfsr & 0x80000000U) != 0U;
        lfsr <<= 1;
        if (msb_set) {
            lfsr ^= mask;
        }
    }

    key[0] = (uint8_t)((lfsr >> 24) & 0xFF);
    key[1] = (uint8_t)((lfsr >> 16) & 0xFF);
    key[2] = (uint8_t)((lfsr >> 8) & 0xFF);
    key[3] = (uint8_t)(lfsr & 0xFF);
    *key_len = SEED_KEY_BYTES;

    ESP_LOGI(TAG, "Generated key (level=0x%02X, mask=0x%08lX):", level, (unsigned long)mask);
    ESP_LOGI(TAG, "  Seed: %02X %02X %02X %02X", seed[0], seed[1], seed[2], seed[3]);
    ESP_LOGI(TAG, "  Key:  %02X %02X %02X %02X", key[0], key[1], key[2], key[3]);

    return ESP_OK;
}
