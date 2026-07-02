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

/* Stage 2 (tenth -> degrees) behaviour: trim, reduce, clamp, disconnect. */
static void test_tenth_to_temp_stages(void)
{
    uint8 ok = 0U;
    TEST_ASSERT_EQUAL_UINT16(5U, tempCalibTenthToTempC(50U, &ok));   /* no trim <10C */
    TEST_ASSERT_EQUAL_UINT8(1U, ok);
    TEST_ASSERT_EQUAL_UINT16(202U, tempCalibTenthToTempC(2050U, &ok)); /* 2050-25=2025 ->202 */
    TEST_ASSERT_EQUAL_UINT8(1U, ok);
    TEST_ASSERT_EQUAL_UINT16(HOST_TEMP_CLAMP_MAX, tempCalibTenthToTempC(4000U, &ok)); /* >390 clamp */
    TEST_ASSERT_EQUAL_UINT8(1U, ok);
    TEST_ASSERT_EQUAL_UINT16(HOST_TEMP_DISCONNECT, tempCalibTenthToTempC(9999U, &ok)); /* disconnect */
    TEST_ASSERT_EQUAL_UINT8(0U, ok);
}

/* The convenience wrapper equals stage1 |> stage2 exactly. */
static void test_raw_wrapper_matches_split(void)
{
    int32_t raws[] = {0, 4200000, 5000000, 6000000, 8388607};
    unsigned i;
    for (i = 0U; i < sizeof(raws) / sizeof(raws[0]); i++)
    {
        uint8 okA = 0U, okB = 0U;
        uint16 a = tempCalibRawToTempC(raws[i], &okA);
        uint16 b = tempCalibTenthToTempC(tempCalibRawToTenthC(raws[i]), &okB);
        TEST_ASSERT_EQUAL_UINT16(b, a);
        TEST_ASSERT_EQUAL_UINT8(okB, okA);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_valid_midrange_points);
    RUN_TEST(test_underrange_reports_disconnect);
    RUN_TEST(test_negative_raw_treated_as_disconnect);
    RUN_TEST(test_overrange_reports_disconnect);
    RUN_TEST(test_null_readok_pointer_is_safe);
    RUN_TEST(test_tenth_to_temp_stages);
    RUN_TEST(test_raw_wrapper_matches_split);
    return UNITY_END();
}
