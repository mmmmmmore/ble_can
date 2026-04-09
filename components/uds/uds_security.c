#include "uds_security.h"
#include "esp_log.h"
#include "mbedtls/md.h"
#include <string.h>

static const char *TAG = "UDS_SECURITY";

// External functions from uds_protocol.c
extern void uds_set_security_level(uint8_t level);

static uds_security_key_gen_cb_t key_gen_callback = NULL;

esp_err_t uds_security_register_key_gen_callback(uds_security_key_gen_cb_t callback)
{
    if (callback == NULL) {
        ESP_LOGE(TAG, "Invalid callback");
        return ESP_ERR_INVALID_ARG;
    }
    
    key_gen_callback = callback;
    ESP_LOGI(TAG, "Security key generation callback registered");
    return ESP_OK;
}

esp_err_t uds_security_request_seed(uint8_t level, uint8_t *seed, uint16_t *seed_len)
{
    if (seed == NULL || seed_len == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (level != 1 && level != 2) {
        ESP_LOGE(TAG, "Invalid security level: %d", level);
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Requesting seed for security level %d", level);
    
    // Prepare sub-function byte for request seed
    uint8_t sub_func = (level == 1) ? UDS_SECURITY_REQUEST_SEED_L1 : UDS_SECURITY_REQUEST_SEED_L2;
    uint8_t data[1] = { sub_func };
    
    uds_response_t response;
    esp_err_t ret = uds_send_single_frame(UDS_SID_SECURITY_ACCESS, data, 1, &response);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send security access request");
        return ret;
    }
    
    if (response.is_negative) {
        ESP_LOGE(TAG, "Negative response to seed request: NRC=0x%02X", response.nrc);
        return ESP_FAIL;
    }
    
    // Response format: [subfunction] [seed...]
    // response.data[0] = subfunction (echo of request)
    // response.data[1...] = actual seed bytes
    if (response.data_len < 1) {
        ESP_LOGE(TAG, "Empty seed response");
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    uint8_t resp_subfunc = response.data[0];
    uint16_t actual_seed_len = response.data_len - 1;  // Exclude subfunction byte
    const uint8_t *actual_seed = &response.data[1];
    
    ESP_LOGI(TAG, "Seed response: subfunc=0x%02X, seed_len=%d", resp_subfunc, actual_seed_len);
    
    // Check if already unlocked (seed = all zeros)
    if (actual_seed_len > 0) {
        bool all_zero = true;
        for (int i = 0; i < actual_seed_len; i++) {
            if (actual_seed[i] != 0x00) {
                all_zero = false;
                break;
            }
        }
        
        if (all_zero) {
            ESP_LOGI(TAG, "Security level %d already unlocked", level);
            uds_set_security_level(level);
            *seed_len = 0;
            return ESP_OK;
        }
    }
    
    // Copy seed (excluding subfunction byte)
    *seed_len = actual_seed_len;
    memcpy(seed, actual_seed, actual_seed_len);
    
    ESP_LOGI(TAG, "Received seed (67 %02X + %d bytes):", resp_subfunc, *seed_len);
    ESP_LOG_BUFFER_HEX(TAG, seed, *seed_len);
    
    return ESP_OK;
}

esp_err_t uds_security_send_key(uint8_t level, const uint8_t *key, uint16_t key_len)
{
    if (key == NULL || key_len == 0) {
        ESP_LOGE(TAG, "Invalid key");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (level != 1 && level != 2) {
        ESP_LOGE(TAG, "Invalid security level: %d", level);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Prepare sub-function byte for send key (27 02 or 27 04)
    uint8_t sub_func = (level == 1) ? UDS_SECURITY_SEND_KEY_L1 : UDS_SECURITY_SEND_KEY_L2;
    
    // Build payload: [subfunction] [key...]
    uint8_t payload[8];  // Max 7 bytes data for single frame: 1 subfunc + up to 6 key bytes
    if (key_len > 6) {
        ESP_LOGE(TAG, "Key too long for single frame: %d", key_len);
        return ESP_ERR_INVALID_SIZE;
    }
    payload[0] = sub_func;
    memcpy(&payload[1], key, key_len);
    
    ESP_LOGI(TAG, "Sending 27 %02X + key:", sub_func);
    ESP_LOG_BUFFER_HEX(TAG, payload, 1 + key_len);
    
    uds_response_t response;
    esp_err_t ret = uds_send_single_frame(UDS_SID_SECURITY_ACCESS, payload, 1 + key_len, &response);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send key");
        return ret;
    }
    
    if (response.is_negative) {
        ESP_LOGE(TAG, "Negative response to key: NRC=0x%02X", response.nrc);
        
        if (response.nrc == UDS_NRC_INVALID_KEY) {
            ESP_LOGE(TAG, "Invalid key!");
        } else if (response.nrc == UDS_NRC_EXCEED_NUMBER_OF_ATTEMPTS) {
            ESP_LOGE(TAG, "Exceeded number of attempts!");
        } else if (response.nrc == UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED) {
            ESP_LOGE(TAG, "Required time delay not expired!");
        }
        
        return ESP_FAIL;
    }
    
    // Security access granted
    uds_set_security_level(level);
    ESP_LOGI(TAG, "Security level %d unlocked successfully", level);
    
    return ESP_OK;
}

esp_err_t uds_security_unlock(uint8_t level)
{
    if (key_gen_callback == NULL) {
        ESP_LOGE(TAG, "No key generation callback registered");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting security unlock procedure for level %d", level);
    
    // Step 1: Request seed (typically 4 bytes for automotive)
    uint8_t seed[8];
    uint16_t seed_len;
    
    esp_err_t ret = uds_security_request_seed(level, seed, &seed_len);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Check if already unlocked
    if (seed_len == 0) {
        return ESP_OK;
    }
    
    // Step 2: Generate key (typically 4 bytes)
    uint8_t key[8];
    uint16_t key_len;
    
    ret = key_gen_callback(seed, seed_len, key, &key_len, level);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Key generation failed");
        return ret;
    }
    
    // Step 3: Send key
    ret = uds_security_send_key(level, key, key_len);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ESP_LOGI(TAG, "Security unlock completed successfully");
    return ESP_OK;
}

// Default security algorithm (FOR TESTING ONLY - Replace with proprietary algorithm)
esp_err_t uds_security_default_key_gen(const uint8_t *seed, uint16_t seed_len, 
                                       uint8_t *key, uint16_t *key_len, 
                                       uint8_t level)
{
    if (seed == NULL || key == NULL || key_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGW(TAG, "Using default security algorithm (TESTING ONLY)");
    
    // Simple XOR with constant (THIS IS NOT SECURE - FOR TESTING ONLY)
    // In production, replace with your proprietary security algorithm
    const uint8_t xor_key_l1[] = { 0x12, 0x34, 0x56, 0x78 };
    const uint8_t xor_key_l2[] = { 0x9A, 0xBC, 0xDE, 0xF0 };
    
    const uint8_t *xor_key = (level == 1) ? xor_key_l1 : xor_key_l2;
    uint8_t xor_key_len = 4;
    
    *key_len = seed_len;
    
    for (int i = 0; i < seed_len; i++) {
        key[i] = seed[i] ^ xor_key[i % xor_key_len];
    }
    
    return ESP_OK;
}
