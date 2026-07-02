/**
 * @file    testSpiFraming.c
 * @brief   Unity tests for the NSS-driven SPI frame-alignment / self-heal logic.
 * @author  Cursor Agent
 * @date    2026-07-02
 */
#include "unity.h"
#include "spiFraming.h"

void setUp(void) {}
void tearDown(void) {}

/* Healthy frame: assert -> complete -> deassert must NOT force a resync,
 * and must report exactly one ready frame. */
static void test_healthy_frame_no_resync(void)
{
    SpiFraming f;
    spiFramingInit(&f);
    spiFramingOnCsAssert(&f);
    spiFramingOnComplete(&f);
    TEST_ASSERT_EQUAL_UINT8(0U, spiFramingOnCsDeassert(&f));
    TEST_ASSERT_EQUAL_UINT8(0U, spiFramingConsumeResync(&f));
    TEST_ASSERT_EQUAL_UINT8(1U, spiFramingConsumeFrameReady(&f));
    TEST_ASSERT_EQUAL_UINT8(0U, spiFramingConsumeFrameReady(&f));
}

/* Short/aborted frame: assert -> (no complete) -> deassert must request a
 * resync so the master can self-heal, and must NOT report a ready frame. */
static void test_short_frame_requests_resync(void)
{
    SpiFraming f;
    spiFramingInit(&f);
    spiFramingOnCsAssert(&f);
    TEST_ASSERT_EQUAL_UINT8(1U, spiFramingOnCsDeassert(&f));
    TEST_ASSERT_EQUAL_UINT8(1U, spiFramingConsumeResync(&f));
    TEST_ASSERT_EQUAL_UINT8(0U, spiFramingConsumeResync(&f));   /* cleared */
    TEST_ASSERT_EQUAL_UINT8(0U, spiFramingConsumeFrameReady(&f));
}

static void test_error_requests_resync_and_clears_active(void)
{
    SpiFraming f;
    spiFramingInit(&f);
    spiFramingOnCsAssert(&f);
    spiFramingOnError(&f);
    TEST_ASSERT_EQUAL_UINT8(1U, spiFramingConsumeResync(&f));
    /* After an error the transaction is no longer active -> update allowed. */
    TEST_ASSERT_EQUAL_UINT8(1U, spiFramingAllowResponseUpdate(&f));
}

/* The response buffer may be updated only while no transaction is active,
 * so the TX bytes cannot tear mid-frame. */
static void test_response_update_gate(void)
{
    SpiFraming f;
    spiFramingInit(&f);
    TEST_ASSERT_EQUAL_UINT8(1U, spiFramingAllowResponseUpdate(&f));
    spiFramingOnCsAssert(&f);
    TEST_ASSERT_EQUAL_UINT8(0U, spiFramingAllowResponseUpdate(&f));
    spiFramingOnComplete(&f);
    TEST_ASSERT_EQUAL_UINT8(1U, spiFramingAllowResponseUpdate(&f));
}

/* A recovered link: stuck frame -> resync -> next frame healthy. */
static void test_recovery_after_short_frame(void)
{
    SpiFraming f;
    spiFramingInit(&f);
    spiFramingOnCsAssert(&f);
    (void)spiFramingOnCsDeassert(&f);              /* short -> resync pending */
    TEST_ASSERT_EQUAL_UINT8(1U, spiFramingConsumeResync(&f));
    /* next transaction completes normally */
    spiFramingOnCsAssert(&f);
    spiFramingOnComplete(&f);
    TEST_ASSERT_EQUAL_UINT8(0U, spiFramingOnCsDeassert(&f));
    TEST_ASSERT_EQUAL_UINT8(1U, spiFramingConsumeFrameReady(&f));
    TEST_ASSERT_EQUAL_UINT8(0U, spiFramingConsumeResync(&f));
}

static void test_null_safe(void)
{
    spiFramingInit(NULL);
    spiFramingOnCsAssert(NULL);
    spiFramingOnComplete(NULL);
    spiFramingOnError(NULL);
    TEST_ASSERT_EQUAL_UINT8(0U, spiFramingOnCsDeassert(NULL));
    TEST_ASSERT_EQUAL_UINT8(0U, spiFramingConsumeResync(NULL));
    TEST_ASSERT_EQUAL_UINT8(0U, spiFramingConsumeFrameReady(NULL));
    TEST_ASSERT_EQUAL_UINT8(0U, spiFramingAllowResponseUpdate(NULL));
    TEST_PASS();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_healthy_frame_no_resync);
    RUN_TEST(test_short_frame_requests_resync);
    RUN_TEST(test_error_requests_resync_and_clears_active);
    RUN_TEST(test_response_update_gate);
    RUN_TEST(test_recovery_after_short_frame);
    RUN_TEST(test_null_safe);
    return UNITY_END();
}
