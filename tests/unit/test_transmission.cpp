#include "core/transmission.h"

#include <gtest/gtest.h>

using namespace exhaust;

class TransmissionTest : public ::testing::Test {
protected:
    Transmission::Config config;
    void SetUp() override
    {
        config.num_gears = 6;
        config.rpm_idle = 800.0f;
        config.rpm_redline = 8000.0f;
        config.rpm_upshift = 0.85f;
        config.rpm_downshift = 0.35f;
    }
};

TEST_F(TransmissionTest, InitialState)
{
    Transmission trans(config);
    EXPECT_EQ(trans.gear(), 1);
    EXPECT_FLOAT_EQ(trans.rpm(), config.rpm_idle);
}

TEST_F(TransmissionTest, ThrottleIncreasesRpm)
{
    Transmission trans(config);
    float initial_rpm = trans.rpm();

    // Apply full throttle for a bit
    for (int i = 0; i < 100; ++i) {
        trans.update(1.0f, 0.016f);
    }

    EXPECT_GT(trans.rpm(), initial_rpm);
}

TEST_F(TransmissionTest, NoThrottleReturnsToIdle)
{
    Transmission trans(config);

    // Rev up
    for (int i = 0; i < 50; ++i) {
        trans.update(1.0f, 0.016f);
    }

    // Release throttle
    for (int i = 0; i < 200; ++i) {
        trans.update(0.0f, 0.016f);
    }

    // Should be near idle
    EXPECT_LT(trans.rpm(), config.rpm_idle + 500.0f);
}

TEST_F(TransmissionTest, ManualShiftUp)
{
    Transmission trans(config);
    EXPECT_EQ(trans.gear(), 1);

    trans.shift_up();
    EXPECT_EQ(trans.gear(), 2);
}

TEST_F(TransmissionTest, ManualShiftDown)
{
    Transmission trans(config);
    trans.shift_up(); // Go to 2nd
    trans.update(0.5f, 0.5f); // Wait for shift to complete

    trans.shift_down();
    EXPECT_EQ(trans.gear(), 1);
}

TEST_F(TransmissionTest, CannotShiftBelowFirst)
{
    Transmission trans(config);
    EXPECT_EQ(trans.gear(), 1);

    trans.shift_down();
    EXPECT_EQ(trans.gear(), 1); // Should stay at 1
}

TEST_F(TransmissionTest, CannotShiftAboveMax)
{
    Transmission trans(config);

    // Shift to top gear
    for (uint8_t i = 0; i < config.num_gears; ++i) {
        trans.shift_up();
        trans.update(0.5f, 0.5f); // Wait for shift
    }

    uint8_t top = trans.gear();
    trans.shift_up();
    EXPECT_EQ(trans.gear(), top); // Should stay at top
}

TEST_F(TransmissionTest, AutoUpshift)
{
    // Use a config where auto-upshift triggers but doesn't immediately downshift
    Transmission::Config auto_config = config;
    auto_config.rpm_downshift = 0.1f; // Very low downshift threshold
    Transmission trans(auto_config);
    EXPECT_EQ(trans.gear(), 1);

    uint8_t max_gear_seen = 1;
    // Full throttle until auto-upshift
    for (int i = 0; i < 1000; ++i) {
        trans.update(1.0f, 0.016f);
        if (trans.gear() > max_gear_seen)
            max_gear_seen = trans.gear();
    }

    // Should have shifted up at least once
    EXPECT_GT(max_gear_seen, 1);
}

TEST_F(TransmissionTest, Reset)
{
    Transmission trans(config);

    // Change state
    for (int i = 0; i < 100; ++i) {
        trans.update(1.0f, 0.016f);
    }

    trans.reset();
    EXPECT_EQ(trans.gear(), 1);
    EXPECT_FLOAT_EQ(trans.rpm(), config.rpm_idle);
    EXPECT_FLOAT_EQ(trans.load(), 0.0f);
}

TEST_F(TransmissionTest, LoadFollowsThrottle)
{
    Transmission trans(config);

    trans.update(0.7f, 0.016f);
    EXPECT_FLOAT_EQ(trans.load(), 0.7f);

    trans.update(0.0f, 0.016f);
    EXPECT_FLOAT_EQ(trans.load(), 0.0f);
}

TEST_F(TransmissionTest, RpmNeverExceedsRedline)
{
    Transmission trans(config);

    // Disable auto-shift by setting very high threshold
    Transmission::Config no_auto = config;
    no_auto.rpm_upshift = 1.1f; // Never triggers
    Transmission trans2(no_auto);

    for (int i = 0; i < 1000; ++i) {
        trans2.update(1.0f, 0.016f);
    }

    EXPECT_LE(trans2.rpm(), config.rpm_redline);
}
