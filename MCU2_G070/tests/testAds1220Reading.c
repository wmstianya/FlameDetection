/**
 * @file    testAds1220Reading.c
 * @brief   Unity tests for the A1 dead-sensor / staleness policy.
 * @author  Cursor Agent
 * @date    2026-07-02
 */
#include "unity.h"
#include "ads1220Reading.h"
#include "hostProtocol.h"

void setUp(void) {}
void tearDown(void) {}

static void test_valid_read_updates_and_reports_ok(void)
{
    Ads1220Reading st;
    uint8 stat = 0xAAU;
    ads1220ReadingInit(&st, 300U);
    uint16 t = ads1220ReadingResolve(&st, ADS1220_PORT_OK, 250U, 1U, 1000U, &stat);
    TEST_ASSERT_EQUAL_UINT16(250U, t);
    TEST_ASSERT_EQUAL_UINT8(HOST_STAT_OK, stat);
}

static void test_not_ready_within_window_holds_last_good(void)
{
    Ads1220Reading st;
    uint8 stat = 0U;
    ads1220ReadingInit(&st, 300U);
    (void)ads1220ReadingResolve(&st, ADS1220_PORT_OK, 275U, 1U, 1000U, &stat);
    uint16 t = ads1220ReadingResolve(&st, ADS1220_PORT_NOT_READY, 0U, 0U, 1500U, &stat);
    TEST_ASSERT_EQUAL_UINT16(275U, t);
    TEST_ASSERT_EQUAL_UINT8(HOST_STAT_OK, stat);
}

static void test_not_ready_beyond_timeout_faults(void)
{
    Ads1220Reading st;
    uint8 stat = 0U;
    ads1220ReadingInit(&st, 300U);
    (void)ads1220ReadingResolve(&st, ADS1220_PORT_OK, 275U, 1U, 1000U, &stat);
    uint16 t = ads1220ReadingResolve(&st, ADS1220_PORT_NOT_READY, 0U, 0U,
                                     1000U + ADS1220_STALE_TIMEOUT_MS + 1U, &stat);
    TEST_ASSERT_EQUAL_UINT16(HOST_TEMP_DISCONNECT, t);
    TEST_ASSERT_EQUAL_UINT8(HOST_STAT_FAULT, stat);
}

/* Core A1 regression: an ADS1220 that is never ready must NOT report the
 * warm-up seed forever; it must fault once the hold window expires. */
static void test_dead_sensor_from_boot_faults_after_window(void)
{
    Ads1220Reading st;
    uint8 stat = 0U;
    ads1220ReadingInit(&st, 300U);
    (void)ads1220ReadingResolve(&st, ADS1220_PORT_NOT_READY, 0U, 0U, 10U, &stat);
    uint16 t = ads1220ReadingResolve(&st, ADS1220_PORT_NOT_READY, 0U, 0U,
                                     ADS1220_STALE_TIMEOUT_MS + 100U, &stat);
    TEST_ASSERT_EQUAL_UINT16(HOST_TEMP_DISCONNECT, t);
    TEST_ASSERT_EQUAL_UINT8(HOST_STAT_FAULT, stat);
}

static void test_invalid_conversion_faults(void)
{
    Ads1220Reading st;
    uint8 stat = 0U;
    ads1220ReadingInit(&st, 300U);
    uint16 t = ads1220ReadingResolve(&st, ADS1220_PORT_OK, 0U, 0U, 500U, &stat);
    TEST_ASSERT_EQUAL_UINT16(HOST_TEMP_DISCONNECT, t);
    TEST_ASSERT_EQUAL_UINT8(HOST_STAT_FAULT, stat);
}

static void test_hard_error_faults(void)
{
    Ads1220Reading st;
    uint8 stat = 0U;
    ads1220ReadingInit(&st, 300U);
    uint16 t = ads1220ReadingResolve(&st, ADS1220_PORT_ERROR, 0U, 0U, 500U, &stat);
    TEST_ASSERT_EQUAL_UINT16(HOST_TEMP_DISCONNECT, t);
    TEST_ASSERT_EQUAL_UINT8(HOST_STAT_FAULT, stat);
}

static void test_null_state_is_safe_fault(void)
{
    uint8 stat = 0U;
    uint16 t = ads1220ReadingResolve(NULL, ADS1220_PORT_OK, 250U, 1U, 100U, &stat);
    TEST_ASSERT_EQUAL_UINT16(HOST_TEMP_DISCONNECT, t);
    TEST_ASSERT_EQUAL_UINT8(HOST_STAT_FAULT, stat);
}

static void test_good_read_recovers_from_fault(void)
{
    Ads1220Reading st;
    uint8 stat = 0U;
    ads1220ReadingInit(&st, 300U);
    (void)ads1220ReadingResolve(&st, ADS1220_PORT_NOT_READY, 0U, 0U,
                                ADS1220_STALE_TIMEOUT_MS + 50U, &stat);
    TEST_ASSERT_EQUAL_UINT8(HOST_STAT_FAULT, stat);
    uint16 t = ads1220ReadingResolve(&st, ADS1220_PORT_OK, 180U, 1U,
                                     ADS1220_STALE_TIMEOUT_MS + 60U, &stat);
    TEST_ASSERT_EQUAL_UINT16(180U, t);
    TEST_ASSERT_EQUAL_UINT8(HOST_STAT_OK, stat);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_valid_read_updates_and_reports_ok);
    RUN_TEST(test_not_ready_within_window_holds_last_good);
    RUN_TEST(test_not_ready_beyond_timeout_faults);
    RUN_TEST(test_dead_sensor_from_boot_faults_after_window);
    RUN_TEST(test_invalid_conversion_faults);
    RUN_TEST(test_hard_error_faults);
    RUN_TEST(test_null_state_is_safe_fault);
    RUN_TEST(test_good_read_recovers_from_fault);
    return UNITY_END();
}
