#include "uds_services.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "UDS_SERVICES";

// External functions from uds_protocol.c
extern void uds_set_session(uint8_t session);

esp_err_t uds_diagnostic_session_control(uint8_t session_type, uds_response_t *response)
{
    ESP_LOGI(TAG, "Diagnostic Session Control: session=0x%02X", session_type);
    
    uint8_t data[1] = { session_type };
    esp_err_t ret = uds_send_single_frame(UDS_SID_DIAGNOSTIC_SESSION_CONTROL, data, 1, response);
    
    if (ret == ESP_OK && response != NULL && !response->is_negative) {
        // Update internal session state
        uds_set_session(session_type);
    }
    
    return ret;
}

esp_err_t uds_ecu_reset(uint8_t reset_type, uds_response_t *response)
{
    ESP_LOGI(TAG, "ECU Reset: type=0x%02X", reset_type);
    
    uint8_t data[1] = { reset_type };
    return uds_send_single_frame(UDS_SID_ECU_RESET, data, 1, response);
}

esp_err_t uds_tester_present(bool suppress_response, uds_response_t *response)
{
    ESP_LOGD(TAG, "Tester Present: suppress=%d", suppress_response);
    
    uint8_t data[1] = { suppress_response ? 0x80 : 0x00 };
    
    if (suppress_response) {
        return uds_send_single_frame(UDS_SID_TESTER_PRESENT, data, 1, NULL);
    } else {
        return uds_send_single_frame(UDS_SID_TESTER_PRESENT, data, 1, response);
    }
}

esp_err_t uds_read_data_by_identifier(uint16_t did, uds_response_t *response)
{
    ESP_LOGI(TAG, "Read Data By Identifier: DID=0x%04X", did);
    
    uint8_t data[2];
    data[0] = (did >> 8) & 0xFF;  // High byte
    data[1] = did & 0xFF;          // Low byte
    
    return uds_send_single_frame(UDS_SID_READ_DATA_BY_IDENTIFIER, data, 2, response);
}

esp_err_t uds_write_data_by_identifier(uint16_t did, const uint8_t *data, uint16_t data_len, uds_response_t *response)
{
    ESP_LOGI(TAG, "Write Data By Identifier: DID=0x%04X, Len=%d", did, data_len);
    
    if (data_len > 4093) {  // Max UDS data minus 2 bytes for DID
        ESP_LOGE(TAG, "Data too long");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Create request
    uds_request_t request;
    request.sid = UDS_SID_WRITE_DATA_BY_IDENTIFIER;
    request.data[0] = (did >> 8) & 0xFF;
    request.data[1] = did & 0xFF;
    
    if (data != NULL && data_len > 0) {
        memcpy(&request.data[2], data, data_len);
    }
    request.data_len = 2 + data_len;
    
    return uds_send_request(&request, response);
}

esp_err_t uds_clear_diagnostic_information(uint32_t group_of_dtc, uds_response_t *response)
{
    ESP_LOGI(TAG, "Clear Diagnostic Information: Group=0x%06lX", group_of_dtc);
    
    uint8_t data[3];
    data[0] = (group_of_dtc >> 16) & 0xFF;
    data[1] = (group_of_dtc >> 8) & 0xFF;
    data[2] = group_of_dtc & 0xFF;
    
    return uds_send_single_frame(UDS_SID_CLEAR_DIAGNOSTIC_INFORMATION, data, 3, response);
}

esp_err_t uds_read_dtc_information(uint8_t sub_function, uint8_t dtc_status_mask, uds_response_t *response)
{
    ESP_LOGI(TAG, "Read DTC Information: Sub=0x%02X, Mask=0x%02X", sub_function, dtc_status_mask);
    
    uint8_t data[2];
    data[0] = sub_function;
    data[1] = dtc_status_mask;
    
    return uds_send_single_frame(UDS_SID_READ_DTC_INFORMATION, data, 2, response);
}

esp_err_t uds_routine_control(uint8_t sub_function, uint16_t routine_id, const uint8_t *data, uint16_t data_len, uds_response_t *response)
{
    ESP_LOGI(TAG, "Routine Control: Sub=0x%02X, ID=0x%04X", sub_function, routine_id);
    
    if (data_len > 4092) {  // Max UDS data minus 3 bytes for sub-function and routine ID
        ESP_LOGE(TAG, "Data too long");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Create request
    uds_request_t request;
    request.sid = UDS_SID_ROUTINE_CONTROL;
    request.data[0] = sub_function;
    request.data[1] = (routine_id >> 8) & 0xFF;
    request.data[2] = routine_id & 0xFF;
    
    if (data != NULL && data_len > 0) {
        memcpy(&request.data[3], data, data_len);
    }
    request.data_len = 3 + data_len;
    
    return uds_send_request(&request, response);
}
