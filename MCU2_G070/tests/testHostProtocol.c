/**
 * @file    testHostProtocol.c
 * @brief   Unity unit tests for the 5-byte host/slave SPI protocol.
 * @author  Cursor Agent
 * @date    2026-07-02
 */
#include "unity.h"
#include "hostProtocol.h"

void setUp(void) {}
void tearDown(void) {}

static void test_checksum_is_sum_of_first_four(void)
{
    uint8 f[HOST_FRAME_LEN] = {0x68U, 0x01U, 0x2CU, 0x01U, 0x00U};
    TEST_ASSERT_EQUAL_UINT8((uint8)(0x68U + 0x01U + 0x2CU + 0x01U),
                            hostFrameChecksum(f));
}

static void test_valid_frame_accepts_good_head_and_csum(void)
{
    uint8 f[HOST_FRAME_LEN] = {0x68U, 0x01U, 0x2CU, 0x01U, 0U};
    f[4] = hostFrameChecksum(f);
    TEST_ASSERT_EQUAL_UINT8(1U, hostFrameValid(f));
}

static void test_valid_frame_rejects_bad_head(void)
{
    uint8 f[HOST_FRAME_LEN] = {0x67U, 0x01U, 0x2CU, 0x01U, 0U};
    f[4] = hostFrameChecksum(f);
    TEST_ASSERT_EQUAL_UINT8(0U, hostFrameValid(f));
}

static void test_valid_frame_rejects_bad_csum(void)
{
    uint8 f[HOST_FRAME_LEN] = {0x68U, 0x01U, 0x2CU, 0x01U, 0xFFU};
    TEST_ASSERT_EQUAL_UINT8(0U, hostFrameValid(f));
}

static void test_build_response_layout_big_endian_temp(void)
{
    HostFrame tx;
    hostBuildResponse(&tx, 300U, HOST_STAT_OK);   /* 300 == 0x012C */
    TEST_ASSERT_EQUAL_UINT8(HOST_FRAME_HEAD, tx.bytes[0]);
    TEST_ASSERT_EQUAL_UINT8(0x01U, tx.bytes[1]);
    TEST_ASSERT_EQUAL_UINT8(0x2CU, tx.bytes[2]);
    TEST_ASSERT_EQUAL_UINT8(HOST_STAT_OK, tx.bytes[3]);
    TEST_ASSERT_EQUAL_UINT8(hostFrameChecksum(tx.bytes), tx.bytes[4]);
}

static void test_build_then_validate_roundtrip(void)
{
    HostFrame tx;
    hostBuildResponse(&tx, 377U, HOST_STAT_FAULT);
    TEST_ASSERT_EQUAL_UINT8(1U, hostFrameValid(tx.bytes));
}

static void test_parse_command_extracts_fields(void)
{
    HostFrame rx;
    HostCommand cmd = {0};
    rx.bytes[0] = HOST_FRAME_HEAD;
    rx.bytes[1] = 0x01U;   /* protect hi */
    rx.bytes[2] = 0x90U;   /* protect lo -> 0x0190 = 400 */
    rx.bytes[3] = 0x01U;   /* yuRe enabled */
    rx.bytes[4] = hostFrameChecksum(rx.bytes);
    TEST_ASSERT_EQUAL_UINT8(1U, hostParseCommand(&rx, &cmd));
    TEST_ASSERT_EQUAL_UINT16(400U, cmd.protectLimitC);
    TEST_ASSERT_EQUAL_UINT8(1U, cmd.yuReEnabled);
}

static void test_parse_command_rejects_invalid_frame(void)
{
    HostFrame rx;
    HostCommand cmd = {0};
    rx.bytes[0] = 0x00U;
    rx.bytes[1] = 0U;
    rx.bytes[2] = 0U;
    rx.bytes[3] = 0U;
    rx.bytes[4] = 0xFFU;
    TEST_ASSERT_EQUAL_UINT8(0U, hostParseCommand(&rx, &cmd));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_checksum_is_sum_of_first_four);
    RUN_TEST(test_valid_frame_accepts_good_head_and_csum);
    RUN_TEST(test_valid_frame_rejects_bad_head);
    RUN_TEST(test_valid_frame_rejects_bad_csum);
    RUN_TEST(test_build_response_layout_big_endian_temp);
    RUN_TEST(test_build_then_validate_roundtrip);
    RUN_TEST(test_parse_command_extracts_fields);
    RUN_TEST(test_parse_command_rejects_invalid_frame);
    return UNITY_END();
}
