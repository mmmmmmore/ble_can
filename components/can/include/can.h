#ifndef CAN_H
#define CAN_H

#include <stdbool.h>

#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_err.h"

/* CAN Configuration */
#define CAN_TX_GPIO_NUM    GPIO_NUM_5
#define CAN_RX_GPIO_NUM    GPIO_NUM_4
#define CAN_BITRATE        TWAI_TIMING_CONFIG_500KBITS()
#define CAN_TWAI_MODE      TWAI_MODE_NORMAL

/* CAN IDs for the current ESP32 <-> ECU setup */
#define CAN_ECU_TX_ID      0x79E  // ESP32 -> ECU
#define CAN_ECU_RX_ID      0x7DE  // ECU -> ESP32
#define CAN_ESP32_RX_ID    0x78E  // External tool -> ESP32
#define CAN_ESP32_TX_ID    0x7CE  // ESP32 -> external tool

/* Active raw-CAN bridge route used by BLE forwarding */
#define CAN_DEFAULT_TX_ID  CAN_ECU_TX_ID
#define CAN_DEFAULT_RX_ID  CAN_ECU_RX_ID

/* Optional compatibility aliases for the UDS helper component */
#define UDS_REQUEST_ID     CAN_ECU_TX_ID
#define UDS_RESPONSE_ID    CAN_ECU_RX_ID

esp_err_t can_driver_init(void);
bool can_driver_is_initialized(void);
esp_err_t can_send_message(uint32_t id, const uint8_t *data, uint8_t len, uint32_t timeout_ms);
esp_err_t can_receive_message(uint32_t *id, uint8_t *data, uint8_t *len, uint32_t timeout_ms);
esp_err_t can_driver_deinit(void);

#endif // CAN_H
