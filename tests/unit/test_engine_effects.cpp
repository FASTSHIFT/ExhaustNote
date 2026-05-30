#include "core/engine_effects.h"

#include <cmath>
#include <cstring>
#include <gtest/gtest.h>

using namespace exhaust;

// ============================================================
// CombustionPulse Tests
// ============================================================

TEST(CombustionPulseTest, ProducesPulsesAtCorrectRate)
{
    // At 1200 RPM with 4 cylinders: 1200*4/120 = 40 firings/sec
    // At 44100 Hz: interval = 44100/40 = 1102.5 samples
    // In 4410 samples (100ms), expect ~4 pulses
    sample_t pulse_data[64];
    for (int i = 0; i < 64; ++i)
        pulse_data[i] = static_cast<sample_t>(10000.0f * std::exp(-i * 0.1f));

    CombustionPulse cp;
    cp.load_pulse(pulse_data, 64);
    CombustionPulse::Config config;
    config.cylinders = 4;
    config.volume = 1.0f;
    config.rpm_fade_start = 8000.0f;
    config.rpm_fade_end = 9000.0f;
    cp.set_config(config);

    float output[4410] = {};
    cp.process(output, 4410, 1200.0f, 44100);

    // Count peaks (pulse onsets)
    int peaks = 0;
    for (int i = 1; i < 4410; ++i) {
        if (output[i] > 5000.0f && output[i - 1] <= 5000.0f)
            peaks++;
    }

    // Expect approximately 4 pulses in 100ms at 40 firings/sec
    EXPECT_GE(peaks, 3);
    EXPECT_LE(peaks, 5);
}

TEST(CombustionPulseTest, FadesOutAtHighRpm)
{
    sample_t pulse_data[32];
    for (int i = 0; i < 32; ++i)
        pulse_data[i] = 10000;

    CombustionPulse cp;
    cp.load_pulse(pulse_data, 32);
    CombustionPulse::Config config;
    config.cylinders = 8;
    config.volume = 1.0f;
    config.rpm_fade_start = 4000.0f;
    config.rpm_fade_end = 6000.0f;
    cp.set_config(config);

    // At 3000 RPM: full volume
    float output_low[2048] = {};
    cp.process(output_low, 2048, 3000.0f, 44100);
    float max_low = 0;
    for (int i = 0; i < 2048; ++i)
        if (std::fabs(output_low[i]) > max_low)
            max_low = std::fabs(output_low[i]);

    // At 7000 RPM: should be silent (above fade_end)
    cp.reset();
    float output_high[2048] = {};
    cp.process(output_high, 2048, 7000.0f, 44100);
    float max_high = 0;
    for (int i = 0; i < 2048; ++i)
        if (std::fabs(output_high[i]) > max_high)
            max_high = std::fabs(output_high[i]);

    EXPECT_GT(max_low, 1000.0f);
    EXPECT_LT(max_high, 1.0f);
}

TEST(CombustionPulseTest, NoPulseWithoutLoad)
{
    CombustionPulse cp;
    // No pulse loaded
    float output[512] = {};
    cp.process(output, 512, 3000.0f, 44100);

    // Should produce nothing
    for (int i = 0; i < 512; ++i) {
        EXPECT_FLOAT_EQ(output[i], 0.0f);
    }
}

// ============================================================
// IdleFluctuation Tests
// ============================================================

TEST(IdleFluctuationTest, ProducesFluctuationAtIdle)
{
    IdleFluctuation fluct;
    IdleFluctuation::Config config;
    config.amplitude = 30.0f;
    config.rate = 1.0f;
    config.rpm_threshold = 1500.0f;
    fluct.set_config(config);

    float max_offset = 0;
    for (int i = 0; i < 100; ++i) {
        float offset = fluct.update(900.0f, 0.016f);
        if (std::fabs(offset) > max_offset)
            max_offset = std::fabs(offset);
    }

    // Should produce some fluctuation
    EXPECT_GT(max_offset, 5.0f);
    EXPECT_LT(max_offset, 50.0f); // But not too much
}

TEST(IdleFluctuationTest, NoFluctuationAtHighRpm)
{
    IdleFluctuation fluct;
    IdleFluctuation::Config config;
    config.amplitude = 30.0f;
    config.rate = 1.0f;
    config.rpm_threshold = 1500.0f;
    fluct.set_config(config);

    float max_offset = 0;
    for (int i = 0; i < 100; ++i) {
        float offset = fluct.update(5000.0f, 0.016f);
        if (std::fabs(offset) > max_offset)
            max_offset = std::fabs(offset);
    }

    // Should be zero above threshold
    EXPECT_FLOAT_EQ(max_offset, 0.0f);
}

// ============================================================
// ThrottleTransient Tests
// ============================================================

TEST(ThrottleTransientTest, AttackOnThrottleOnset)
{
    ThrottleTransient trans;
    ThrottleTransient::Config config;
    config.attack_gain = 1.5f;
    config.attack_time = 0.05f;
    config.decay_time = 0.2f;
    trans.set_config(config);

    // Idle (no throttle)
    float gain = trans.update(0.0f, 0.016f);
    EXPECT_NEAR(gain, 1.0f, 0.1f);

    // Sudden throttle application
    gain = trans.update(0.8f, 0.016f);
    EXPECT_GT(gain, 1.2f); // Should be boosted
}

TEST(ThrottleTransientTest, DecaysBackToUnity)
{
    ThrottleTransient trans;
    ThrottleTransient::Config config;
    config.attack_gain = 1.5f;
    config.attack_time = 0.05f;
    config.decay_time = 0.2f;
    trans.set_config(config);

    // Trigger attack
    trans.update(0.0f, 0.016f);
    trans.update(0.8f, 0.016f);

    // Wait for decay (500ms total)
    float gain = 1.0f;
    for (int i = 0; i < 30; ++i) {
        gain = trans.update(0.8f, 0.016f);
    }

    // Should have decayed back near 1.0
    EXPECT_NEAR(gain, 1.0f, 0.15f);
}

TEST(ThrottleTransientTest, NoAttackOnSteadyThrottle)
{
    ThrottleTransient trans;
    ThrottleTransient::Config config;
    config.attack_gain = 1.5f;
    config.attack_time = 0.05f;
    config.decay_time = 0.2f;
    trans.set_config(config);

    // Initialize at 0.5 throttle (first call triggers attack due to delta)
    trans.update(0.5f, 0.016f);
    // Wait for decay
    for (int i = 0; i < 30; ++i)
        trans.update(0.5f, 0.016f);

    // Now steady throttle should be near 1.0
    for (int i = 0; i < 5; ++i) {
        float gain = trans.update(0.5f, 0.016f);
        EXPECT_NEAR(gain, 1.0f, 0.1f);
    }
}

// ============================================================
// RevLimiter Tests
// ============================================================

TEST(RevLimiterTest, NormalBelowLimit)
{
    RevLimiter limiter;
    RevLimiter::Config config;
    config.rpm_limit = 9000.0f;
    config.rpm_hysteresis = 100.0f;
    config.cut_probability = 0.7f;
    limiter.set_config(config);

    // Below limit: always returns 1.0
    for (int i = 0; i < 100; ++i) {
        float gain = limiter.update(7000.0f);
        EXPECT_FLOAT_EQ(gain, 1.0f);
    }
}

TEST(RevLimiterTest, CutsAtLimit)
{
    RevLimiter limiter;
    RevLimiter::Config config;
    config.rpm_limit = 9000.0f;
    config.rpm_hysteresis = 100.0f;
    config.cut_probability = 0.7f;
    limiter.set_config(config);

    // At limit: should produce some cuts (gain < 1.0)
    int cuts = 0;
    for (int i = 0; i < 100; ++i) {
        float gain = limiter.update(9100.0f);
        if (gain < 0.5f)
            cuts++;
    }

    // With 70% probability, expect majority to be cuts
    EXPECT_GT(cuts, 30);
    // Allow up to 100% cuts (PRNG dependent)
}

TEST(RevLimiterTest, HysteresisWorks)
{
    RevLimiter limiter;
    RevLimiter::Config config;
    config.rpm_limit = 9000.0f;
    config.rpm_hysteresis = 200.0f;
    config.cut_probability = 1.0f; // Always cut when active
    limiter.set_config(config);

    // Activate
    limiter.update(9100.0f);
    float gain = limiter.update(9100.0f);
    EXPECT_LT(gain, 0.5f); // Should be cutting

    // Drop below limit but above hysteresis: still active
    gain = limiter.update(8900.0f);
    EXPECT_LT(gain, 0.5f); // Still cutting (within hysteresis)

    // Drop below hysteresis: deactivate
    gain = limiter.update(8700.0f);
    EXPECT_FLOAT_EQ(gain, 1.0f); // Normal again
}
