#ifndef UDS_PROTOCOL_H
#define UDS_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* UDS Service IDs (ISO 14229-1) */
#define UDS_SID_DIAGNOSTIC_SESSION_CONTROL      0x10
#define UDS_SID_ECU_RESET                       0x11
#define UDS_SID_SECURITY_ACCESS                 0x27
#define UDS_SID_COMMUNICATION_CONTROL           0x28
#define UDS_SID_TESTER_PRESENT                  0x3E
#define UDS_SID_READ_DATA_BY_IDENTIFIER         0x22
#define UDS_SID_READ_MEMORY_BY_ADDRESS          0x23
#define UDS_SID_WRITE_DATA_BY_IDENTIFIER        0x2E
#define UDS_SID_WRITE_MEMORY_BY_ADDRESS         0x3D
#define UDS_SID_CLEAR_DIAGNOSTIC_INFORMATION    0x14
#define UDS_SID_READ_DTC_INFORMATION            0x19
#define UDS_SID_IO_CONTROL_BY_IDENTIFIER        0x2F
#define UDS_SID_ROUTINE_CONTROL                 0x31
#define UDS_SID_REQUEST_DOWNLOAD                0x34
#define UDS_SID_REQUEST_UPLOAD                  0x35
#define UDS_SID_TRANSFER_DATA                   0x36
#define UDS_SID_REQUEST_TRANSFER_EXIT           0x37

/* Positive Response Offset */
#define UDS_POSITIVE_RESPONSE_OFFSET            0x40

/* Negative Response Code */
#define UDS_SID_NEGATIVE_RESPONSE               0x7F

/* Negative Response Codes (NRC) */
#define UDS_NRC_GENERAL_REJECT                  0x10
#define UDS_NRC_SERVICE_NOT_SUPPORTED           0x11
#define UDS_NRC_SUBFUNCTION_NOT_SUPPORTED       0x12
#define UDS_NRC_INCORRECT_MESSAGE_LENGTH        0x13
#define UDS_NRC_CONDITIONS_NOT_CORRECT          0x22
#define UDS_NRC_REQUEST_SEQUENCE_ERROR          0x24
#define UDS_NRC_REQUEST_OUT_OF_RANGE            0x31
#define UDS_NRC_SECURITY_ACCESS_DENIED          0x33
#define UDS_NRC_INVALID_KEY                     0x35
#define UDS_NRC_EXCEED_NUMBER_OF_ATTEMPTS       0x36
#define UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED 0x37
#define UDS_NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED    0x70
#define UDS_NRC_TRANSFER_DATA_SUSPENDED         0x71
#define UDS_NRC_GENERAL_PROGRAMMING_FAILURE     0x72
#define UDS_NRC_RESPONSE_PENDING                0x78  // requestCorrectlyReceived-ResponsePending

/* Diagnostic Session Types */
#define UDS_SESSION_DEFAULT                     0x01
#define UDS_SESSION_PROGRAMMING                 0x02
#define UDS_SESSION_EXTENDED_DIAGNOSTIC         0x03

/* Security Access Levels */
#define UDS_SECURITY_LEVEL_LOCKED               0x00
#define UDS_SECURITY_LEVEL_1                    0x01
#define UDS_SECURITY_LEVEL_2                    0x02

/* UDS Message Structure */
typedef struct {
    uint8_t sid;            // Service ID
    uint8_t data[4095];     // Data payload (max for ISO-TP)
    uint16_t data_len;      // Data length
} uds_request_t;

typedef struct {
    uint8_t sid;            // Response SID (request SID + 0x40 for positive)
    uint8_t data[4095];     // Response data
    uint16_t data_len;      // Response data length
    bool is_negative;       // True if negative response
    uint8_t nrc;            // Negative response code (if is_negative)
} uds_response_t;

/* UDS Configuration */
typedef struct {
    uint32_t request_id;    // CAN ID for requests
    uint32_t response_id;   // CAN ID for responses
    uint32_t timeout_ms;    // Response timeout in milliseconds
} uds_config_t;

/**
 * @brief Initialize UDS protocol stack
 * 
 * @param config UDS configuration
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t uds_init(const uds_config_t *config);

/**
 * @brief Set UDS response timeout
 * @param timeout_ms Timeout in milliseconds
 */
void uds_set_timeout(uint32_t timeout_ms);

/**
 * @brief Get current UDS response timeout
 * @return Timeout in milliseconds
 */
uint32_t uds_get_timeout(void);

/**
 * @brief Send UDS request and wait for response
 * 
 * @param request Pointer to UDS request
 * @param response Pointer to store UDS response
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t uds_send_request(const uds_request_t *request, uds_response_t *response);

/**
 * @brief Send single frame UDS request (data <= 7 bytes)
 * 
 * @param sid Service ID
 * @param data Pointer to data buffer
 * @param data_len Data length
 * @param response Pointer to store UDS response
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t uds_send_single_frame(uint8_t sid, const uint8_t *data, uint8_t data_len, uds_response_t *response);

/**
 * @brief Get current diagnostic session
 * 
 * @return Current session type
 */
uint8_t uds_get_current_session(void);

/**
 * @brief Get current security level
 * 
 * @return Current security level
 */
uint8_t uds_get_security_level(void);

/**
 * @brief Deinitialize UDS protocol stack
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t uds_deinit(void);

#endif // UDS_PROTOCOL_H
