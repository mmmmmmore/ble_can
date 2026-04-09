#include "uds_protocol.h"
#include "can.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "UDS_PROTOCOL";

static uds_config_t uds_config;
static bool is_initialized = false;
static uint8_t current_session = UDS_SESSION_DEFAULT;
static uint8_t security_level = UDS_SECURITY_LEVEL_LOCKED;

static esp_err_t uds_receive_isotp_payload(uint8_t request_sid,
                       uint8_t *payload,
                       size_t payload_capacity,
                       size_t *payload_len);
static esp_err_t uds_handle_single_frame(const uint8_t *frame, uint8_t frame_len, uint8_t *payload, size_t payload_capacity, size_t *payload_len);
static esp_err_t uds_handle_multi_frame(const uint8_t *first_frame, uint8_t frame_len, uint8_t *payload, size_t payload_capacity, size_t *payload_len);
static esp_err_t uds_send_flow_control_frame(void);
static esp_err_t uds_send_multi_frame(uint8_t sid, const uint8_t *data, size_t data_len, uds_response_t *response);

esp_err_t uds_init(const uds_config_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&uds_config, config, sizeof(uds_config_t));
    is_initialized = true;
    current_session = UDS_SESSION_DEFAULT;
    security_level = UDS_SECURITY_LEVEL_LOCKED;

    ESP_LOGI(TAG, "UDS protocol initialized (REQ: 0x%03lX, RESP: 0x%03lX, Timeout: %lu ms)",
             config->request_id, config->response_id, config->timeout_ms);

    return ESP_OK;
}

void uds_set_timeout(uint32_t timeout_ms)
{
    uds_config.timeout_ms = timeout_ms;
}

uint32_t uds_get_timeout(void)
{
    return uds_config.timeout_ms;
}

esp_err_t uds_send_single_frame(uint8_t sid, const uint8_t *data, uint8_t data_len, uds_response_t *response)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "UDS not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (data_len > 7) {
        ESP_LOGE(TAG, "Data length exceeds single frame capacity (7 bytes)");
        return ESP_ERR_INVALID_ARG;
    }

    // Prepare single frame CAN message
    uint8_t can_data[8];
    can_data[0] = data_len + 1;  // PCI byte: single frame, length = SID + data
    can_data[1] = sid;
    
    if (data != NULL && data_len > 0) {
        memcpy(&can_data[2], data, data_len);
    }

    // Pad remaining bytes with 0xCC (ISO 15765-2 recommendation)
    for (int i = 2 + data_len; i < 8; i++) {
        can_data[i] = 0xCC;
    }

    // Log the actual UDS command bytes
    char cmd_str[32] = {0};
    int pos = snprintf(cmd_str, sizeof(cmd_str), "%02X", sid);
    for (int i = 0; i < data_len && pos < (int)sizeof(cmd_str) - 3; i++) {
        pos += snprintf(cmd_str + pos, sizeof(cmd_str) - pos, " %02X", data[i]);
    }
    ESP_LOGI(TAG, "Sending UDS request: %s", cmd_str);

    // Send CAN message
    esp_err_t ret = can_send_message(uds_config.request_id, can_data, 8, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send UDS request");
        return ret;
    }

    // Wait for response if required
    if (response == NULL) {
        return ESP_OK;  // No response expected
    }

    size_t payload_capacity = sizeof(response->data) + 1;  // include SID
    uint8_t *payload = (uint8_t *)malloc(payload_capacity);
    if (payload == NULL) {
        ESP_LOGE(TAG, "No memory for UDS response buffer");
        return ESP_ERR_NO_MEM;
    }

    size_t payload_len = 0;
    ret = uds_receive_isotp_payload(sid, payload, payload_capacity, &payload_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to receive UDS response: %s", esp_err_to_name(ret));
        free(payload);
        return ret;
    }

    if (payload_len == 0) {
        free(payload);
        return ESP_ERR_INVALID_SIZE;
    }

    response->sid = payload[0];
    response->data_len = payload_len - 1;

    if (response->data_len > sizeof(response->data)) {
        response->data_len = sizeof(response->data);
    }
    if (response->data_len > 0) {
        memcpy(response->data, &payload[1], response->data_len);
    }

    if (response->sid == UDS_SID_NEGATIVE_RESPONSE) {
        response->is_negative = true;
        uint8_t request_sid = (response->data_len >= 1) ? response->data[0] : 0;
        response->nrc = (response->data_len >= 2) ? response->data[1] : 0;
        
        // NRC 0x78 = Response Pending - keep waiting for actual response
        // ECU may send multiple 0x78 responses before the final answer
        while (response->nrc == UDS_NRC_RESPONSE_PENDING) {
            ESP_LOGI(TAG, "Response pending (NRC=0x78), waiting for final response...");
            // Continue receiving - the actual response will follow
            size_t pending_payload_len = 0;
            ret = uds_receive_isotp_payload(sid, payload, payload_capacity, &pending_payload_len);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to receive final response after NRC 0x78: %s", esp_err_to_name(ret));
                free(payload);
                return ret;
            }
            // Re-parse the response
            response->sid = payload[0];
            response->data_len = pending_payload_len - 1;
            if (response->data_len > sizeof(response->data)) {
                response->data_len = sizeof(response->data);
            }
            if (response->data_len > 0) {
                memcpy(response->data, &payload[1], response->data_len);
            }
            // Check if still negative (could be another 0x78 or final negative)
            if (response->sid == UDS_SID_NEGATIVE_RESPONSE) {
                response->is_negative = true;
                request_sid = (response->data_len >= 1) ? response->data[0] : 0;
                response->nrc = (response->data_len >= 2) ? response->data[1] : 0;
                if (response->nrc != UDS_NRC_RESPONSE_PENDING) {
                    ESP_LOGW(TAG, "Negative response after pending: SID=0x%02X, NRC=0x%02X", request_sid, response->nrc);
                }
                // If NRC is still 0x78, loop will continue
            } else {
                response->is_negative = false;
                ESP_LOGI(TAG, "Positive response after pending: SID=0x%02X, Len=%d", response->sid, response->data_len);
                free(payload);
                return ESP_OK;
            }
        }
        
        // Final response is negative (not 0x78)
        ESP_LOGW(TAG, "Negative response: SID=0x%02X, NRC=0x%02X", request_sid, response->nrc);
    } else {
        response->is_negative = false;
        ESP_LOGI(TAG, "Positive response: SID=0x%02X, Len=%d", response->sid, response->data_len);
    }

    free(payload);
    return ESP_OK;
}

esp_err_t uds_send_request(const uds_request_t *request, uds_response_t *response)
{
    if (request == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Single frame: SID (1 byte) + data (up to 6 bytes) = 7 bytes max in payload
    // Total CAN frame: PCI (1 byte) + payload = 8 bytes
    if (request->data_len <= 6) {
        return uds_send_single_frame(request->sid, request->data, request->data_len, response);
    } else {
        // Multi-frame required for longer payloads
        return uds_send_multi_frame(request->sid, request->data, request->data_len, response);
    }
}

uint8_t uds_get_current_session(void)
{
    return current_session;
}

uint8_t uds_get_security_level(void)
{
    return security_level;
}

esp_err_t uds_deinit(void)
{
    is_initialized = false;
    current_session = UDS_SESSION_DEFAULT;
    security_level = UDS_SECURITY_LEVEL_LOCKED;
    
    ESP_LOGI(TAG, "UDS protocol deinitialized");
    return ESP_OK;
}

// Internal functions to update session and security level
void uds_set_session(uint8_t session)
{
    current_session = session;
    ESP_LOGI(TAG, "Session changed to: 0x%02X", session);
}

void uds_set_security_level(uint8_t level)
{
    security_level = level;
    ESP_LOGI(TAG, "Security level changed to: 0x%02X", level);
}

static bool uds_sid_matches(uint8_t request_sid, uint8_t response_sid)
{
    if (response_sid == UDS_SID_NEGATIVE_RESPONSE) {
        return true;
    }
    uint8_t expected = (uint8_t)(request_sid + UDS_POSITIVE_RESPONSE_OFFSET);
    return response_sid == expected;
}

static esp_err_t uds_receive_isotp_payload(uint8_t request_sid,
                       uint8_t *payload,
                       size_t payload_capacity,
                       size_t *payload_len)
{
    if (payload == NULL || payload_capacity == 0 || payload_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t rx_id = 0;
    uint8_t rx_data[8] = {0};
    uint8_t rx_len = 0;

	while (1) {
	    esp_err_t ret = can_receive_message(&rx_id, rx_data, &rx_len, uds_config.timeout_ms);
        if (ret != ESP_OK) {
            return ret;
        }

        if (rx_id != uds_config.response_id) {
            continue;
        }

        uint8_t pci_type = rx_data[0] & 0xF0;
        if (pci_type != 0x00 && pci_type != 0x10) {
            if (pci_type == 0x20) {
            ESP_LOGW(TAG, "Unexpected consecutive frame received before first frame");
            } else if (pci_type == 0x30) {
            ESP_LOGW(TAG, "Flow control frame received unexpectedly");
            } else {
            ESP_LOGW(TAG, "Unsupported PCI type 0x%02X", pci_type);
            }
            continue;
        }

        ret = (pci_type == 0x00)
	    ? uds_handle_single_frame(rx_data, rx_len, payload, payload_capacity, payload_len)
	    : uds_handle_multi_frame(rx_data, rx_len, payload, payload_capacity, payload_len);
        if (ret != ESP_OK) {
            return ret;
        }

        if (*payload_len == 0) {
            continue;
        }

        uint8_t response_sid = payload[0];
        if (uds_sid_matches(request_sid, response_sid)) {
            return ESP_OK;
        }

        ESP_LOGW(TAG,
             "Ignoring SID 0x%02X while waiting for request 0x%02X",
             response_sid,
             request_sid);
        // keep waiting for the matching response
    }
}

static esp_err_t uds_handle_single_frame(const uint8_t *frame, uint8_t frame_len,
                                         uint8_t *payload, size_t payload_capacity, size_t *payload_len)
{
    if (frame_len < 2) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t len = frame[0] & 0x0F;
    if (len == 0 || len > (frame_len - 1) || len > payload_capacity) {
        return ESP_ERR_INVALID_SIZE;
    }

    memcpy(payload, &frame[1], len);
    *payload_len = len;
    return ESP_OK;
}

static esp_err_t uds_handle_multi_frame(const uint8_t *first_frame, uint8_t frame_len,
                                        uint8_t *payload, size_t payload_capacity, size_t *payload_len)
{
    if (frame_len < 3) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint16_t total_len = ((first_frame[0] & 0x0F) << 8) | first_frame[1];
    if (total_len == 0 || total_len > payload_capacity) {
        return ESP_ERR_INVALID_SIZE;
    }

    size_t bytes_copied = frame_len - 2;
    if (bytes_copied > total_len) {
        bytes_copied = total_len;
    }
    memcpy(payload, &first_frame[2], bytes_copied);

    esp_err_t ret = uds_send_flow_control_frame();
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t expected_seq = 1;
    while (bytes_copied < total_len) {
        uint32_t rx_id = 0;
        uint8_t rx_data[8] = {0};
        uint8_t rx_len = 0;

        ret = can_receive_message(&rx_id, rx_data, &rx_len, uds_config.timeout_ms);
        if (ret != ESP_OK) {
            return ret;
        }
        if (rx_id != uds_config.response_id) {
            continue;
        }

        uint8_t pci = rx_data[0] & 0xF0;
        if (pci != 0x20) {
            ESP_LOGW(TAG, "Unexpected PCI 0x%02X during multi-frame reception", pci);
            continue;
        }

        uint8_t seq = rx_data[0] & 0x0F;
        if (seq != expected_seq) {
            ESP_LOGW(TAG, "Sequence mismatch: got %u expected %u", seq, expected_seq);
            expected_seq = (seq + 1) & 0x0F;
        } else {
            expected_seq = (expected_seq + 1) & 0x0F;
        }

        if (rx_len <= 1) {
            continue;
        }

        size_t chunk_len = rx_len - 1;
        size_t remaining = total_len - bytes_copied;
        if (chunk_len > remaining) {
            chunk_len = remaining;
        }

        memcpy(&payload[bytes_copied], &rx_data[1], chunk_len);
        bytes_copied += chunk_len;
    }

    *payload_len = total_len;
    return ESP_OK;
}

static esp_err_t uds_send_flow_control_frame(void)
{
    uint8_t fc[8] = {0x30, 0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC};
    return can_send_message(uds_config.request_id, fc, 8, 100);
}

// Wait for Flow Control frame from ECU and extract parameters
static esp_err_t uds_wait_flow_control(uint8_t *block_size, uint8_t *st_min)
{
    uint32_t rx_id = 0;
    uint8_t rx_data[8] = {0};
    uint8_t rx_len = 0;
    
    while (1) {
        esp_err_t ret = can_receive_message(&rx_id, rx_data, &rx_len, uds_config.timeout_ms);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Timeout waiting for Flow Control");
            return ret;
        }
        
        if (rx_id != uds_config.response_id) {
            continue;
        }
        
        uint8_t pci_type = rx_data[0] & 0xF0;
        if (pci_type == 0x30) {
            // Flow Control frame
            uint8_t flow_status = rx_data[0] & 0x0F;
            if (flow_status == 0) {
                // CTS (Clear To Send)
                *block_size = rx_data[1];
                *st_min = rx_data[2];
                ESP_LOGD(TAG, "Flow Control: BS=%d, STmin=%d", *block_size, *st_min);
                return ESP_OK;
            } else if (flow_status == 1) {
                // Wait
                ESP_LOGD(TAG, "Flow Control: Wait");
                continue;
            } else {
                // Overflow or abort
                ESP_LOGE(TAG, "Flow Control: Overflow/Abort (status=%d)", flow_status);
                return ESP_FAIL;
            }
        }
        // Ignore other frames while waiting for FC
    }
}

// Send multi-frame ISO-TP request
static esp_err_t uds_send_multi_frame(uint8_t sid, const uint8_t *data, size_t data_len, uds_response_t *response)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "UDS not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Total payload = SID + data
    size_t total_len = 1 + data_len;
    if (total_len > 4095) {
        ESP_LOGE(TAG, "Payload too large for ISO-TP (%zu bytes)", total_len);
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Log the request (first few bytes)
    char cmd_str[48] = {0};
    int pos = snprintf(cmd_str, sizeof(cmd_str), "%02X", sid);
    for (size_t i = 0; i < data_len && i < 10 && pos < (int)sizeof(cmd_str) - 4; i++) {
        pos += snprintf(cmd_str + pos, sizeof(cmd_str) - pos, " %02X", data[i]);
    }
    if (data_len > 10) {
        snprintf(cmd_str + pos, sizeof(cmd_str) - pos, "...");
    }
    ESP_LOGI(TAG, "Sending UDS multi-frame request: %s (total %zu bytes)", cmd_str, total_len);
    
    // Build complete payload (SID + data)
    uint8_t *payload = (uint8_t *)malloc(total_len);
    if (payload == NULL) {
        return ESP_ERR_NO_MEM;
    }
    payload[0] = sid;
    if (data_len > 0) {
        memcpy(&payload[1], data, data_len);
    }
    
    // Send First Frame (FF): PCI = 0x1X XX, then 6 data bytes
    uint8_t can_data[8];
    can_data[0] = 0x10 | ((total_len >> 8) & 0x0F);  // FF PCI high nibble + length high
    can_data[1] = total_len & 0xFF;                   // Length low byte
    size_t ff_data_len = (total_len > 6) ? 6 : total_len;
    memcpy(&can_data[2], payload, ff_data_len);
    // Pad remaining
    for (size_t i = 2 + ff_data_len; i < 8; i++) {
        can_data[i] = 0xCC;
    }
    
    esp_err_t ret = can_send_message(uds_config.request_id, can_data, 8, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send First Frame");
        free(payload);
        return ret;
    }
    
    // Wait for Flow Control
    uint8_t block_size = 0;
    uint8_t st_min = 0;
    ret = uds_wait_flow_control(&block_size, &st_min);
    if (ret != ESP_OK) {
        free(payload);
        return ret;
    }
    
    // Send Consecutive Frames (CF)
    size_t offset = ff_data_len;
    uint8_t seq_num = 1;
    uint8_t block_count = 0;
    
    while (offset < total_len) {
        // Separation time (STmin in ms or us)
        if (st_min > 0 && st_min <= 127) {
            vTaskDelay(pdMS_TO_TICKS(st_min));
        } else if (st_min >= 0xF1 && st_min <= 0xF9) {
            // 100us - 900us, use minimum delay
            vTaskDelay(1);
        }
        
        // Build Consecutive Frame
        can_data[0] = 0x20 | (seq_num & 0x0F);
        size_t chunk_len = total_len - offset;
        if (chunk_len > 7) {
            chunk_len = 7;
        }
        memcpy(&can_data[1], &payload[offset], chunk_len);
        // Pad remaining
        for (size_t i = 1 + chunk_len; i < 8; i++) {
            can_data[i] = 0xCC;
        }
        
        ret = can_send_message(uds_config.request_id, can_data, 8, 100);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send Consecutive Frame %d", seq_num);
            free(payload);
            return ret;
        }
        
        offset += chunk_len;
        seq_num = (seq_num + 1) & 0x0F;
        block_count++;
        
        // Check block size - wait for next FC if needed
        if (block_size > 0 && block_count >= block_size && offset < total_len) {
            ret = uds_wait_flow_control(&block_size, &st_min);
            if (ret != ESP_OK) {
                free(payload);
                return ret;
            }
            block_count = 0;
        }
    }
    
    free(payload);
    ESP_LOGD(TAG, "Multi-frame TX complete (%zu bytes)", total_len);
    
    // Wait for response if required
    if (response == NULL) {
        return ESP_OK;
    }
    
    // Receive response (same as single frame)
    size_t resp_capacity = sizeof(response->data) + 1;
    uint8_t *resp_payload = (uint8_t *)malloc(resp_capacity);
    if (resp_payload == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    size_t resp_len = 0;
    ret = uds_receive_isotp_payload(sid, resp_payload, resp_capacity, &resp_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to receive response: %s", esp_err_to_name(ret));
        free(resp_payload);
        return ret;
    }
    
    if (resp_len == 0) {
        free(resp_payload);
        return ESP_ERR_INVALID_SIZE;
    }
    
    response->sid = resp_payload[0];
    response->data_len = resp_len - 1;
    if (response->data_len > sizeof(response->data)) {
        response->data_len = sizeof(response->data);
    }
    if (response->data_len > 0) {
        memcpy(response->data, &resp_payload[1], response->data_len);
    }
    
    if (response->sid == UDS_SID_NEGATIVE_RESPONSE) {
        response->is_negative = true;
        uint8_t req_sid = (response->data_len >= 1) ? response->data[0] : 0;
        response->nrc = (response->data_len >= 2) ? response->data[1] : 0;
        
        // Handle NRC 0x78 (Response Pending) - loop until final response
        while (response->nrc == UDS_NRC_RESPONSE_PENDING) {
            ESP_LOGI(TAG, "Multi-frame: Response pending (NRC=0x78), waiting for final response...");
            size_t pending_len = 0;
            ret = uds_receive_isotp_payload(sid, resp_payload, resp_capacity, &pending_len);
            if (ret != ESP_OK) {
                free(resp_payload);
                return ret;
            }
            response->sid = resp_payload[0];
            response->data_len = pending_len - 1;
            if (response->data_len > sizeof(response->data)) {
                response->data_len = sizeof(response->data);
            }
            if (response->data_len > 0) {
                memcpy(response->data, &resp_payload[1], response->data_len);
            }
            if (response->sid == UDS_SID_NEGATIVE_RESPONSE) {
                response->is_negative = true;
                req_sid = (response->data_len >= 1) ? response->data[0] : 0;
                response->nrc = (response->data_len >= 2) ? response->data[1] : 0;
                // If NRC is 0x78 again, continue the loop
                if (response->nrc != UDS_NRC_RESPONSE_PENDING) {
                    ESP_LOGW(TAG, "Multi-frame: Negative response after pending: SID=0x%02X, NRC=0x%02X", req_sid, response->nrc);
                }
            } else {
                response->is_negative = false;
                ESP_LOGI(TAG, "Multi-frame: Positive response after pending: SID=0x%02X, Len=%d", response->sid, response->data_len);
                break;  // Exit loop on positive response
            }
        }
        if (response->is_negative && response->nrc != UDS_NRC_RESPONSE_PENDING) {
            ESP_LOGW(TAG, "Negative response: SID=0x%02X, NRC=0x%02X", req_sid, response->nrc);
        }
    } else {
        response->is_negative = false;
        ESP_LOGI(TAG, "Positive response: SID=0x%02X, Len=%d", response->sid, response->data_len);
    }
    
    free(resp_payload);
    return ESP_OK;
}
