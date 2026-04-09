#ifndef UDS_SERVICES_H
#define UDS_SERVICES_H

#include "uds_protocol.h"

/**
 * @brief Send Diagnostic Session Control request
 * 
 * @param session_type Session type (0x01: default, 0x02: programming, 0x03: extended)
 * @param response Pointer to store response
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t uds_diagnostic_session_control(uint8_t session_type, uds_response_t *response);

/**
 * @brief Send ECU Reset request
 * 
 * @param reset_type Reset type (0x01: hard reset, 0x02: key off/on, 0x03: soft reset)
 * @param response Pointer to store response
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t uds_ecu_reset(uint8_t reset_type, uds_response_t *response);

/**
 * @brief Send Tester Present request
 * 
 * @param suppress_response If true, suppress positive response
 * @param response Pointer to store response (NULL if suppress_response is true)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t uds_tester_present(bool suppress_response, uds_response_t *response);

/**
 * @brief Send Read Data By Identifier request
 * 
 * @param did Data Identifier (16-bit)
 * @param response Pointer to store response
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t uds_read_data_by_identifier(uint16_t did, uds_response_t *response);

/**
 * @brief Send Write Data By Identifier request
 * 
 * @param did Data Identifier (16-bit)
 * @param data Pointer to data to write
 * @param data_len Length of data
 * @param response Pointer to store response
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t uds_write_data_by_identifier(uint16_t did, const uint8_t *data, uint16_t data_len, uds_response_t *response);

/**
 * @brief Send Clear Diagnostic Information request
 * 
 * @param group_of_dtc DTC group (0xFFFFFF: all DTCs)
 * @param response Pointer to store response
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t uds_clear_diagnostic_information(uint32_t group_of_dtc, uds_response_t *response);

/**
 * @brief Send Read DTC Information request
 * 
 * @param sub_function Sub-function (e.g., 0x02: report DTC by status mask)
 * @param dtc_status_mask Status mask
 * @param response Pointer to store response
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t uds_read_dtc_information(uint8_t sub_function, uint8_t dtc_status_mask, uds_response_t *response);

/**
 * @brief Send Routine Control request
 * 
 * @param sub_function Sub-function (0x01: start, 0x02: stop, 0x03: request results)
 * @param routine_id Routine Identifier (16-bit)
 * @param data Optional routine data
 * @param data_len Length of optional data
 * @param response Pointer to store response
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t uds_routine_control(uint8_t sub_function, uint16_t routine_id, const uint8_t *data, uint16_t data_len, uds_response_t *response);

#endif // UDS_SERVICES_H
