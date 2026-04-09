#include "unity.h"
#include "can.h"

TEST_CASE("can_send_requires_init", "[can]")
{
    uint8_t data[1] = {0xAA};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, can_send_message(UDS_REQUEST_ID, data, sizeof(data), 10));
}

TEST_CASE("can_receive_requires_init", "[can]")
{
    uint32_t id = 0;
    uint8_t data[8] = {0};
    uint8_t len = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, can_receive_message(&id, data, &len, 10));
}

TEST_CASE("can_deinit_without_init_is_ok", "[can]")
{
    TEST_ASSERT_EQUAL(ESP_OK, can_driver_deinit());
}
