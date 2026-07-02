/**
 * @file    testTempFilter.c
 * @brief   Unity tests for the 21-point moving-average furnace-temp filter.
 * @author  Cursor Agent
 * @date    2026-07-02
 */
#include "unity.h"
#include "tempFilter.h"

void setUp(void) {}
void tearDown(void) {}

static void test_first_sample_returns_itself(void)
{
    TempFilter f;
    tempFilterInit(&f);
    TEST_ASSERT_EQUAL_UINT16(250U, tempFilterPush(&f, 250U));
}

static void test_partial_fill_is_running_average(void)
{
    TempFilter f;
    tempFilterInit(&f);
    TEST_ASSERT_EQUAL_UINT16(100U, tempFilterPush(&f, 100U));
    TEST_ASSERT_EQUAL_UINT16(150U, tempFilterPush(&f, 200U));
    TEST_ASSERT_EQUAL_UINT16(200U, tempFilterPush(&f, 300U));
}

static void test_constant_input_converges(void)
{
    TempFilter f;
    uint8 i;
    uint16 avg = 0U;
    tempFilterInit(&f);
    for (i = 0U; i < 2U * TEMP_FILTER_WINDOW; i++)
        avg = tempFilterPush(&f, 50U);
    TEST_ASSERT_EQUAL_UINT16(50U, avg);
}

static void test_window_slides_after_full(void)
{
    TempFilter f;
    uint8 i;
    tempFilterInit(&f);
    for (i = 0U; i < TEMP_FILTER_WINDOW; i++)
        (void)tempFilterPush(&f, 100U);
    /* window full of 100 -> avg 100; one 1000 replaces one dropped 100:
     * (20*100 + 1000)/21 = 3000/21 = 142 (integer). */
    TEST_ASSERT_EQUAL_UINT16(142U, tempFilterPush(&f, 1000U));
}

static void test_saturation_value_is_stable(void)
{
    TempFilter f;
    uint8 i;
    uint16 avg = 0U;
    tempFilterInit(&f);
    for (i = 0U; i < TEMP_FILTER_WINDOW; i++)
        avg = tempFilterPush(&f, 9999U);
    TEST_ASSERT_EQUAL_UINT16(9999U, avg);
}

static void test_null_filter_passes_sample_through(void)
{
    TEST_ASSERT_EQUAL_UINT16(123U, tempFilterPush(NULL, 123U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_first_sample_returns_itself);
    RUN_TEST(test_partial_fill_is_running_average);
    RUN_TEST(test_constant_input_converges);
    RUN_TEST(test_window_slides_after_full);
    RUN_TEST(test_saturation_value_is_stable);
    RUN_TEST(test_null_filter_passes_sample_through);
    return UNITY_END();
}
