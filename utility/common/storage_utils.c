#include "utility_common.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_spiffs.h"

#define STORAGE_BASE_PATH        "/spiffs"
#define STORAGE_FW_DIR           STORAGE_BASE_PATH "/firmware"
#define STORAGE_PARTITION_LABEL  "spiffs"
#define STORAGE_PATH_MAX         512
#define STORAGE_WRITE_SLICE      (16 * 1024)

static const char *TAG = "UTILITY_STORAGE";
static bool s_spiffs_mounted = false;
static bool s_fw_dir_ready = false;
static bool s_fw_dir_unsupported = false;
static void log_firmware_dir_status(const char *reason);

esp_err_t utility_storage_mount(void)
{
    if (s_spiffs_mounted) {
        return ESP_OK;
    }

    esp_vfs_spiffs_conf_t conf = {
        .base_path = STORAGE_BASE_PATH,
        .partition_label = STORAGE_PARTITION_LABEL,
        .max_files = 8,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret == ESP_OK) {
        s_spiffs_mounted = true;
        ESP_LOGI(TAG, "SPIFFS mounted at %s", STORAGE_BASE_PATH);
    } else {
        ESP_LOGE(TAG, "Failed to mount SPIFFS: %s", esp_err_to_name(ret));
    }
    return ret;
}

static void ensure_firmware_dir(void)
{
    if (s_fw_dir_ready || s_fw_dir_unsupported) {
        return;
    }

    struct stat st = {0};
    if (stat(STORAGE_FW_DIR, &st) == 0 && S_ISDIR(st.st_mode)) {
        s_fw_dir_ready = true;
        log_firmware_dir_status("stat-success");
        return;
    }

    if (mkdir(STORAGE_FW_DIR, 0775) == 0 || errno == EEXIST) {
        s_fw_dir_ready = true;
        log_firmware_dir_status("mkdir-success");
        return;
    }

    if (errno == ENOTSUP || errno == ENOSYS) {
        s_fw_dir_unsupported = true;
        ESP_LOGI(TAG, "Firmware dir creation unsupported; assuming %s exists in image", STORAGE_FW_DIR);
        log_firmware_dir_status("mkdir-unsupported");
        return;
    }

    ESP_LOGE(TAG, "Failed to create firmware dir (%s): %s", STORAGE_FW_DIR, strerror(errno));
    log_firmware_dir_status("mkdir-failed");
}

static void log_firmware_dir_status(const char *reason)
{
    const char *context = (reason != NULL) ? reason : "unknown";
    if (!s_spiffs_mounted) {
        ESP_LOGW(TAG, "Firmware dir status (%s): SPIFFS not mounted (ready=%d unsupported=%d)",
                 context,
                 s_fw_dir_ready,
                 s_fw_dir_unsupported);
        return;
    }

    struct stat st = {0};
    if (stat(STORAGE_FW_DIR, &st) == 0) {
        ESP_LOGI(TAG, "Firmware dir status (%s): exists=%d mode=%03o size=%ld ready=%d unsupported=%d",
                 context,
                 S_ISDIR(st.st_mode) ? 1 : 0,
                 (unsigned int)(st.st_mode & 0777),
                 (long)st.st_size,
                 s_fw_dir_ready,
                 s_fw_dir_unsupported);
    } else {
        ESP_LOGW(TAG, "Firmware dir status (%s): stat failed (%s) ready=%d unsupported=%d",
                 context,
                 strerror(errno),
                 s_fw_dir_ready,
                 s_fw_dir_unsupported);
    }
}

esp_err_t utility_storage_save_firmware(const char *filename, const uint8_t *data, size_t len)
{
    if (!s_spiffs_mounted) {
        ESP_LOGE(TAG, "SPIFFS not mounted");
        return ESP_ERR_INVALID_STATE;
    }
    if (filename == NULL || data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ensure_firmware_dir();

    char path[STORAGE_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", STORAGE_FW_DIR, filename);

    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "Failed to open %s: %s", path, strerror(errno));
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, len, fp);
    fclose(fp);

    if (written != len) {
        ESP_LOGE(TAG, "Short write (%zu/%zu)", written, len);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Firmware saved to %s (%zu bytes)", path, len);
    return ESP_OK;
}

esp_err_t utility_storage_get_firmware_path(const char *filename, char *path_out, size_t path_len)
{
    if (!s_spiffs_mounted) {
        ESP_LOGE(TAG, "SPIFFS not mounted");
        return ESP_ERR_INVALID_STATE;
    }
    if (filename == NULL || filename[0] == '\0' || path_out == NULL || path_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ensure_firmware_dir();

    int written = snprintf(path_out, path_len, "%s/%s", STORAGE_FW_DIR, filename);
    if (written <= 0 || (size_t)written >= path_len) {
        return ESP_ERR_INVALID_SIZE;
    }

    struct stat st = {0};
    if (stat(path_out, &st) != 0 || !S_ISREG(st.st_mode)) {
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

bool utility_storage_firmware_exists(const char *filename)
{
    char path[STORAGE_PATH_MAX];
    return utility_storage_get_firmware_path(filename, path, sizeof(path)) == ESP_OK;
}

esp_err_t utility_storage_clear_firmware_dir(void)
{
    if (!s_spiffs_mounted) {
        ESP_LOGE(TAG, "SPIFFS not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    ensure_firmware_dir();

    DIR *dir = opendir(STORAGE_FW_DIR);
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open %s for clearing: %s", STORAGE_FW_DIR, strerror(errno));
        return ESP_FAIL;
    }

    esp_err_t ret = ESP_OK;
    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char path[STORAGE_PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", STORAGE_FW_DIR, entry->d_name);

        struct stat st = {0};
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }

        if (unlink(path) != 0) {
            ESP_LOGE(TAG, "Failed to remove %s: %s", path, strerror(errno));
            ret = ESP_FAIL;
        } else {
            ESP_LOGI(TAG, "Removed firmware file %s", path);
        }
    }

    closedir(dir);
    return ret;
}

esp_err_t utility_storage_write_firmware_chunk(const char *filename, const uint8_t *data,
                                               size_t len, bool append)
{
    if (!s_spiffs_mounted) {
        ESP_LOGE(TAG, "SPIFFS not mounted");
        return ESP_ERR_INVALID_STATE;
    }
    if (filename == NULL || filename[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    ensure_firmware_dir();

    char path[STORAGE_PATH_MAX];
    int written = snprintf(path, sizeof(path), "%s/%s", STORAGE_FW_DIR, filename);
    if (written <= 0 || (size_t)written >= sizeof(path)) {
        return ESP_ERR_INVALID_SIZE;
    }

    const char *mode = append ? "ab" : "wb";
    FILE *fp = fopen(path, mode);
    if (fp == NULL) {
        ESP_LOGE(TAG, "Failed to open %s for chunk write: %s", path, strerror(errno));
        return ESP_FAIL;
    }

    if (len == 0) {
        fclose(fp);
        return ESP_OK;
    }

    if (data == NULL) {
        fclose(fp);
        return ESP_ERR_INVALID_ARG;
    }

    if (len == 0) {
        fclose(fp);
        return ESP_OK;
    }

    size_t remaining = len;
    size_t written_total = 0;
    while (remaining > 0) {
        size_t slice = remaining;
        if (slice > STORAGE_WRITE_SLICE) {
            slice = STORAGE_WRITE_SLICE;
        }

        size_t chunk_written = fwrite(data + written_total, 1, slice, fp);
        if (chunk_written != slice) {
            int err = errno;
            ESP_LOGE(TAG, "Short chunk write (%zu/%zu) to %s (slice=%zu errno=%d %s)",
                     chunk_written,
                     slice,
                     path,
                     slice,
                     err,
                     strerror(err));
            fclose(fp);
            return ESP_FAIL;
        }

        written_total += chunk_written;
        remaining -= chunk_written;
    }

    fclose(fp);
    return ESP_OK;
}

void utility_storage_log_firmware_dir(void)
{
    if (!s_spiffs_mounted) {
        ESP_LOGW(TAG, "SPIFFS not mounted; cannot list firmware directory");
        return;
    }

    ensure_firmware_dir();
    log_firmware_dir_status("list-start");

    DIR *dir = opendir(STORAGE_FW_DIR);
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open %s for listing: %s", STORAGE_FW_DIR, strerror(errno));
        return;
    }

    ESP_LOGI(TAG, "Firmware directory contents (%s):", STORAGE_FW_DIR);
    bool is_empty = true;
    struct dirent *entry = NULL;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char path[STORAGE_PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", STORAGE_FW_DIR, entry->d_name);

        struct stat st = {0};
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }

        is_empty = false;
        ESP_LOGI(TAG, "  %s (%ld bytes)", entry->d_name, (long)st.st_size);
    }

    if (is_empty) {
        ESP_LOGI(TAG, "  <empty>");
    }

    closedir(dir);
}
