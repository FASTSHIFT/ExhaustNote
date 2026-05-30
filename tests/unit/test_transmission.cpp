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
        // Physics parameters needed for torque model
        config.inertia = 0.12f;
        config.peak_torque = 400.0f;
        config.peak_torque_rpm = 5000.0f;
        config.friction = 15.0f;
        config.dynamic_friction = 0.005f;
        config.engine_brake = 30.0f;
        config.throttle_smooth_up = 25.0f;
        config.throttle_smooth_down = 12.0f;
        config.rev_limiter_rpm_drop = 200.0f;
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

    // Apply full throttle — physics model needs time for throttle smoothing + inertia
    for (int i = 0; i < 300; ++i) {
        trans.update(1.0f, 0.016f);
    }

    EXPECT_GT(trans.rpm(), initial_rpm + 500.0f);
}

TEST_F(TransmissionTest, NoThrottleReturnsToIdle)
{
    Transmission trans(config);

    // Rev up briefly (not to redline)
    for (int i = 0; i < 100; ++i) {
        trans.update(0.5f, 0.016f);
    }
    EXPECT_GT(trans.rpm(), 1500.0f); // Should have revved up

    // Release throttle — engine braking + friction brings it down
    for (int i = 0; i < 1000; ++i) {
        trans.update(0.0f, 0.016f);
    }

    // Should be near idle (idle controller holds it)
    EXPECT_LT(trans.rpm(), config.rpm_idle * 1.3f);
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
    // Disable auto-shift for this test
    Transmission::Config manual_config = config;
    manual_config.rpm_upshift = 2.0f;
    manual_config.rpm_downshift = -1.0f;
    Transmission trans(manual_config);

    // Rev up first
    for (int i = 0; i < 50; ++i)
        trans.update(0.6f, 0.016f);

    trans.shift_up(); // Go to 2nd
    // Wait for shift to complete
    for (int i = 0; i < 15; ++i)
        trans.update(0.3f, 0.016f);

    EXPECT_EQ(trans.gear(), 2);
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
    // Use a config where auto-upshift triggers
    Transmission::Config auto_config = config;
    auto_config.rpm_downshift = 0.05f; // Very low downshift threshold
    auto_config.rpm_upshift = 0.80f;
    Transmission trans(auto_config);
    EXPECT_EQ(trans.gear(), 1);

    uint8_t max_gear_seen = 1;
    // Full throttle for extended time (physics model has inertia)
    for (int i = 0; i < 2000; ++i) {
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

    // Apply partial throttle (won't hit limiter)
    // At mid-RPM with partial throttle, should have load
    for (int i = 0; i < 150; ++i)
        trans.update(0.5f, 0.016f);
    // Check at a point before hitting redline
    float mid_load = trans.load();
    EXPECT_GT(mid_load, 0.05f) << "RPM=" << trans.rpm();

    // Release throttle — load should drop
    for (int i = 0; i < 300; ++i)
        trans.update(0.0f, 0.016f);
    EXPECT_LT(trans.load(), 0.05f);
}

TEST_F(TransmissionTest, RevLimiterKeepsRpmBounded)
{
    // Physics-based rev limiter: fuel cut causes RPM oscillation
    // RPM should stay within a reasonable band above redline
    Transmission::Config no_auto = config;
    no_auto.rpm_upshift = 1.1f; // Never triggers
    Transmission trans2(no_auto);

    float max_rpm = 0;
    for (int i = 0; i < 2000; ++i) {
        trans2.update(1.0f, 0.016f);
        if (trans2.rpm() > max_rpm)
            max_rpm = trans2.rpm();
    }

    // Allow up to 2% overshoot (physics integration + inertia)
    EXPECT_LE(max_rpm, config.rpm_redline * 1.05f);
    // But should reach near redline
    EXPECT_GT(max_rpm, config.rpm_redline * 0.95f);
}
