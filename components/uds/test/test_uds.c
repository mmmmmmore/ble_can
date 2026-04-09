#include "unity.h"
#include "uds_protocol.h"

TEST_CASE("uds_init_rejects_null_config", "[uds]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, uds_init(NULL));
}

TEST_CASE("uds_timeout_roundtrip", "[uds]")
{
    uds_config_t cfg = {
        .request_id = 0x7DF,
        .response_id = 0x7E8,
        .timeout_ms = 100
    };

    TEST_ASSERT_EQUAL(ESP_OK, uds_init(&cfg));
    uds_set_timeout(250);
    TEST_ASSERT_EQUAL_UINT32(250, uds_get_timeout());
    TEST_ASSERT_EQUAL(ESP_OK, uds_deinit());
}

TEST_CASE("uds_send_single_frame_requires_init", "[uds]")
{
    uint8_t payload[2] = {0x01, 0x02};
    uds_response_t response = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, uds_send_single_frame(0x10, payload, sizeof(payload), &response));
}
