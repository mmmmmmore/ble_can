#ifndef UTILITY_COMMON_H
#define UTILITY_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t utility_storage_mount(void);
esp_err_t utility_storage_save_firmware(const char *filename, const uint8_t *data, size_t len);
esp_err_t utility_storage_get_firmware_path(const char *filename, char *path_out, size_t path_len);
bool utility_storage_firmware_exists(const char *filename);
esp_err_t utility_storage_clear_firmware_dir(void);
void utility_storage_log_firmware_dir(void);
esp_err_t utility_storage_write_firmware_chunk(const char *filename, const uint8_t *data,
                                                  size_t len, bool append);

esp_err_t utility_security_generate_key(const uint8_t *seed, uint16_t seed_len,
                                        uint8_t *key, uint16_t *key_len,
                                        uint8_t level);

#ifdef __cplusplus
}
#endif

#endif // UTILITY_COMMON_H
