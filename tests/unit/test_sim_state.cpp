#include "core/transmission.h"

#include <gtest/gtest.h>

// Include the sim modules (header-only state + physics)
// We test the physics logic independently of GUI/SDL
#include "../../app/sim_gui/sim_physics.h"
#include "../../app/sim_gui/sim_state.h"

using namespace exhaust;

class SimPhysicsTest : public ::testing::Test {
protected:
    SimState state;
    Transmission trans;

    void SetUp() override
    {
        Transmission::Config tc;
        tc.num_gears = 6;
        tc.rpm_idle = 800.0f;
        tc.rpm_redline = 8000.0f;
        tc.inertia = 0.12f;
        tc.peak_torque = 400.0f;
        tc.peak_torque_rpm = 5000.0f;
        tc.engine_brake = 30.0f;
        tc.auto_shift = false;
        trans = Transmission(tc);

        state.engine_on = true;
        state.physics.load_nm = 200.0f;
        state.physics.engine_brake_nm = 60.0f;
        state.physics.road_coeff = 0.3f;
        state.physics.brake_force_nm = 400.0f;
    }
};

TEST_F(SimPhysicsTest, ThrottleIncreasesRpm)
{
    state.throttle = 1.0f;
    state.braking = false;

    float initial_rpm = trans.rpm();
    for (int i = 0; i < 100; i++) {
        sim_physics_update(state, trans, 0.016f);
    }
    EXPECT_GT(state.smoothed_rpm, initial_rpm + 500.0f);
}

TEST_F(SimPhysicsTest, BrakeDecreasesRpm)
{
    // First rev up
    state.throttle = 1.0f;
    state.braking = false;
    for (int i = 0; i < 200; i++) {
        sim_physics_update(state, trans, 0.016f);
    }
    float high_rpm = state.smoothed_rpm;
    EXPECT_GT(high_rpm, 3000.0f);

    // Now brake — verify RPM decreases (not necessarily fast due to inertia model)
    state.throttle = 0.0f;
    state.braking = true;
    for (int i = 0; i < 500; i++) {
        sim_physics_update(state, trans, 0.016f);
    }
    EXPECT_LT(state.smoothed_rpm, high_rpm)
        << "Brake should reduce RPM. high=" << high_rpm
        << " now=" << state.smoothed_rpm;
}

TEST_F(SimPhysicsTest, PhysicsConfigApplied)
{
    state.physics.engine_brake_nm = 150.0f;
    state.physics.road_coeff = 0.8f;
    state.throttle = 0.0f;
    state.braking = false;

    sim_physics_update(state, trans, 0.016f);

    // Verify setters were called (check via getters)
    EXPECT_FLOAT_EQ(trans.engine_brake(), 150.0f);
    EXPECT_FLOAT_EQ(trans.road_load_coeff(), 0.8f);
}

TEST_F(SimPhysicsTest, BrakeAppliesBrakeTorque)
{
    state.throttle = 0.0f;
    state.braking = true;
    state.physics.load_nm = 200.0f;
    state.physics.brake_force_nm = 400.0f;

    sim_physics_update(state, trans, 0.016f);

    // Brake torque applied independently (doesn't inflate external_load)
    EXPECT_FLOAT_EQ(trans.external_load(), 200.0f); // Unchanged
    EXPECT_FLOAT_EQ(trans.brake_torque(), 400.0f); // Brake applied separately
}

TEST_F(SimPhysicsTest, NoBrakeNormalLoad)
{
    state.throttle = 0.5f;
    state.braking = false;
    state.physics.load_nm = 200.0f;

    sim_physics_update(state, trans, 0.016f);

    EXPECT_FLOAT_EQ(trans.external_load(), 200.0f);
}

TEST_F(SimPhysicsTest, EngineOffNoUpdate)
{
    state.engine_on = false;
    state.throttle = 1.0f;

    float initial_rpm = trans.rpm();
    for (int i = 0; i < 50; i++) {
        sim_physics_update(state, trans, 0.016f);
    }
    // RPM should not change when engine is off
    EXPECT_FLOAT_EQ(trans.rpm(), initial_rpm);
}

TEST_F(SimPhysicsTest, RpmSmoothingWorks)
{
    // Set smoothed_rpm far from actual to test convergence
    state.smoothed_rpm = 2000.0f;
    state.throttle = 1.0f;
    state.braking = false;

    // Run several frames — smoothed should converge toward actual
    for (int i = 0; i < 50; i++) {
        sim_physics_update(state, trans, 0.016f);
    }
    float actual_rpm = trans.rpm();
    // After 50 frames (800ms), smoothed should be close to actual
    EXPECT_NEAR(state.smoothed_rpm, actual_rpm, actual_rpm * 0.2f);
}
