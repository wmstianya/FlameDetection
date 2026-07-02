/**
 * @file    testTempCalib.c
 * @brief   Unity regression tests locking the ADS1220->temperature conversion.
 * @details Anchor values captured from the reference implementation; they guard
 *          against accidental changes to the calibration table or scaling.
 * @author  Cursor Agent
 * @date    2026-07-02
 */
#include "unity.h"
#include "tempCalib.h"
#include "hostProtocol.h"

void setUp(void) {}
void tearDown(void) {}

static void expectTemp(int32_t raw, uint16 wantTempC, uint8 wantOk)
{
    uint8 ok = 0xAAU;
    uint16 t = tempCalibRawToTempC(raw, &ok);
    TEST_ASSERT_EQUAL_UINT16(wantTempC, t);
    TEST_ASSERT_EQUAL_UINT8(wantOk, ok);
}

static void test_valid_midrange_points(void)
{
    expectTemp(4200000, 62U, 1U);
    expectTemp(4600000, 93U, 1U);
    expectTemp(5000000, 125U, 1U);
    expectTemp(5500000, 165U, 1U);
    expectTemp(6000000, 205U, 1U);
}

static void test_underrange_reports_disconnect(void)
{
    expectTemp(0, HOST_TEMP_DISCONNECT, 0U);
    expectTemp(300000, HOST_TEMP_DISCONNECT, 0U);
}

static void test_negative_raw_treated_as_disconnect(void)
{
    expectTemp(-5, HOST_TEMP_DISCONNECT, 0U);
}

static void test_overrange_reports_disconnect(void)
{
    expectTemp(7000000, HOST_TEMP_DISCONNECT, 0U);
    expectTemp(8388607, HOST_TEMP_DISCONNECT, 0U);
}

static void test_null_readok_pointer_is_safe(void)
{
    (void)tempCalibRawToTempC(5000000, NULL);
    TEST_PASS();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_valid_midrange_points);
    RUN_TEST(test_underrange_reports_disconnect);
    RUN_TEST(test_negative_raw_treated_as_disconnect);
    RUN_TEST(test_overrange_reports_disconnect);
    RUN_TEST(test_null_readok_pointer_is_safe);
    return UNITY_END();
}
