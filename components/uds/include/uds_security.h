#ifndef UDS_SECURITY_H
#define UDS_SECURITY_H

#include "uds_protocol.h"

/* Security Access Sub-functions */
#define UDS_SECURITY_REQUEST_SEED_L1            0x01
#define UDS_SECURITY_SEND_KEY_L1                0x02
#define UDS_SECURITY_REQUEST_SEED_L2            0x03
#define UDS_SECURITY_SEND_KEY_L2                0x04

/**
 * @brief Security key generation callback type
 * 
 * This callback will be called to generate the security key from the seed.
 * You need to implement your proprietary security algorithm here.
 * 
 * @param seed Pointer to seed data
 * @param seed_len Length of seed
 * @param key Pointer to buffer to store generated key
 * @param key_len Pointer to store key length
 * @param level Security level
 * @return ESP_OK on success, error code otherwise
 */
typedef esp_err_t (*uds_security_key_gen_cb_t)(const uint8_t *seed, uint16_t seed_len, 
                                                uint8_t *key, uint16_t *key_len, 
                                                uint8_t level);

/**
 * @brief Register security key generation callback
 * 
 * @param callback Key generation callback function
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t uds_security_register_key_gen_callback(uds_security_key_gen_cb_t callback);

/**
 * @brief Request security seed from ECU
 * 
 * @param level Security level (1 or 2)
 * @param seed Pointer to buffer to store seed
 * @param seed_len Pointer to store seed length
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t uds_security_request_seed(uint8_t level, uint8_t *seed, uint16_t *seed_len);

/**
 * @brief Send security key to ECU
 * 
 * @param level Security level (1 or 2)
 * @param key Pointer to key data
 * @param key_len Length of key
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t uds_security_send_key(uint8_t level, const uint8_t *key, uint16_t key_len);

/**
 * @brief Perform complete security access procedure
 * 
 * This function will:
 * 1. Request seed from ECU
 * 2. Generate key using registered callback
 * 3. Send key to ECU
 * 4. Update security level on success
 * 
 * @param level Security level (1 or 2)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t uds_security_unlock(uint8_t level);

/**
 * @brief Default security algorithm (XOR with constant - FOR TESTING ONLY)
 * 
 * WARNING: This is a simple XOR algorithm for testing purposes only.
 * Replace this with your proprietary security algorithm in production.
 * 
 * @param seed Pointer to seed data
 * @param seed_len Length of seed
 * @param key Pointer to buffer to store generated key
 * @param key_len Pointer to store key length
 * @param level Security level
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t uds_security_default_key_gen(const uint8_t *seed, uint16_t seed_len, 
                                       uint8_t *key, uint16_t *key_len, 
                                       uint8_t level);

#endif // UDS_SECURITY_H
