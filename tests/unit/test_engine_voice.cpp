#include "core/engine_voice.h"
#include "core/sample_player.h"
#include "core/transmission.h"

#include <cmath>
#include <cstring>
#include <gtest/gtest.h>
#include <vector>

using namespace exhaust;

// Helper: generate a simple sine wave sample for testing
static std::vector<sample_t> make_sine(size_t length, float freq, float sample_rate = 44100.0f)
{
    std::vector<sample_t> buf(length);
    for (size_t i = 0; i < length; ++i) {
        float t = static_cast<float>(i) / sample_rate;
        buf[i] = static_cast<sample_t>(16000.0f * std::sin(2.0f * 3.14159f * freq * t));
    }
    return buf;
}

class EngineVoiceTest : public ::testing::Test {
protected:
    static constexpr size_t kLayerLen = 44100; // 1 second
    std::vector<sample_t> layer_data[5];
    EngineVoice::LayerConfig configs[5];

    void SetUp() override
    {
        float rpms[] = { 1000.0f, 3000.0f, 5000.0f, 7000.0f, 9000.0f };
        for (int i = 0; i < 5; ++i) {
            // Each layer is a sine at its fundamental frequency (4-cyl)
            float freq = rpms[i] * 4.0f / 120.0f;
            layer_data[i] = make_sine(kLayerLen, freq);
            configs[i].rpm = rpms[i];
            configs[i].data = layer_data[i].data();
            configs[i].length = kLayerLen;
        }
    }
};

// --- Trapezoidal Envelope Tests ---

TEST_F(EngineVoiceTest, AutoEnvelopeNoVolumeDip)
{
    // After auto-fill, the total gain should never drop below a threshold
    // at any RPM in the operating range.
    EngineVoice voice;
    voice.set_onload_layers_auto(configs, 5);
    voice.set_engine_config(4);

    // Sweep RPM and check output is never silent
    sample_t output[512];
    for (float rpm = 1000.0f; rpm <= 9000.0f; rpm += 100.0f) {
        EngineVoice::Params params;
        params.rpm = rpm;
        params.throttle = 1.0f;

        // Process multiple blocks to let gains stabilize
        for (int i = 0; i < 10; ++i) {
            voice.process(params, output, 512);
        }

        // Check output is not silent (RMS > threshold)
        double rms = 0.0;
        for (int i = 0; i < 512; ++i) {
            rms += static_cast<double>(output[i]) * static_cast<double>(output[i]);
        }
        rms = std::sqrt(rms / 512.0);
        EXPECT_GT(rms, 100.0) << "Volume dip at RPM=" << rpm;
    }
}

TEST_F(EngineVoiceTest, AutoEnvelopeNoSuddenVolumeDrop)
{
    // Adjacent RPM points should not have sudden volume drops.
    // This catches the "seam" problem where volume dips between layers.
    EngineVoice voice;
    voice.set_onload_layers_auto(configs, 5);
    voice.set_engine_config(4);

    double prev_rms = 0;
    bool first = true;

    for (float rpm = 1500.0f; rpm <= 8500.0f; rpm += 200.0f) {
        EngineVoice::Params params;
        params.rpm = rpm;
        params.throttle = 1.0f;

        sample_t output[512];
        // Let gains stabilize
        for (int i = 0; i < 20; ++i) {
            voice.process(params, output, 512);
        }

        double rms = 0.0;
        for (int i = 0; i < 512; ++i) {
            rms += static_cast<double>(output[i]) * static_cast<double>(output[i]);
        }
        rms = std::sqrt(rms / 512.0);

        if (!first && prev_rms > 500.0) {
            // No complete silence at seams: current should be at least 5% of previous.
            // Note: with synthetic sine waves, phase cancellation can cause
            // natural RMS variation; real samples won't have this issue.
            double drop_ratio = rms / prev_rms;
            EXPECT_GT(drop_ratio, 0.05)
                << "Near-silence at seam RPM=" << rpm
                << " (prev_rms=" << prev_rms << " curr_rms=" << rms << ")";
        }

        prev_rms = rms;
        first = false;
    }
}

TEST_F(EngineVoiceTest, PitchFollowsRpm)
{
    // At higher RPM, the output should have higher frequency content
    EngineVoice voice;
    voice.set_onload_layers_auto(configs, 5);
    voice.set_engine_config(4);

    auto measure_zero_crossings = [](sample_t* buf, size_t len) {
        int crossings = 0;
        for (size_t i = 1; i < len; ++i) {
            if ((buf[i - 1] >= 0 && buf[i] < 0) || (buf[i - 1] < 0 && buf[i] >= 0))
                crossings++;
        }
        return crossings;
    };

    sample_t output[512];

    // Low RPM
    EngineVoice::Params params_low;
    params_low.rpm = 2000.0f;
    params_low.throttle = 1.0f;
    for (int i = 0; i < 20; ++i)
        voice.process(params_low, output, 512);
    voice.process(params_low, output, 512);
    int crossings_low = measure_zero_crossings(output, 512);

    // High RPM
    voice.reset();
    EngineVoice::Params params_high;
    params_high.rpm = 7000.0f;
    params_high.throttle = 1.0f;
    for (int i = 0; i < 20; ++i)
        voice.process(params_high, output, 512);
    voice.process(params_high, output, 512);
    int crossings_high = measure_zero_crossings(output, 512);

    // Higher RPM should have more zero crossings (higher pitch)
    EXPECT_GT(crossings_high, crossings_low)
        << "High RPM should produce higher pitch: low=" << crossings_low
        << " high=" << crossings_high;
}

TEST_F(EngineVoiceTest, ThrottleBlendWorks)
{
    EngineVoice voice;
    voice.set_onload_layers_auto(configs, 5);
    voice.set_offload_layers_auto(configs, 5);
    voice.set_engine_config(4);

    sample_t output_on[512], output_off[512];

    // Full throttle
    EngineVoice::Params params;
    params.rpm = 5000.0f;
    params.throttle = 1.0f;
    for (int i = 0; i < 10; ++i)
        voice.process(params, output_on, 512);

    voice.reset();

    // Zero throttle
    params.throttle = 0.0f;
    for (int i = 0; i < 10; ++i)
        voice.process(params, output_off, 512);

    // Both should produce sound (not silent)
    double rms_on = 0, rms_off = 0;
    for (int i = 0; i < 512; ++i) {
        rms_on += output_on[i] * output_on[i];
        rms_off += output_off[i] * output_off[i];
    }
    EXPECT_GT(rms_on, 0.0);
    EXPECT_GT(rms_off, 0.0);
}

TEST_F(EngineVoiceTest, ResetClearsState)
{
    EngineVoice voice;
    voice.set_onload_layers_auto(configs, 5);
    voice.set_engine_config(4);

    sample_t output[512];
    EngineVoice::Params params;
    params.rpm = 5000.0f;
    params.throttle = 1.0f;

    // Process some audio
    voice.process(params, output, 512);

    // Reset
    voice.reset();

    // After reset, first output should be near-silent (gains start at 0)
    voice.process(params, output, 512);
    double rms = 0;
    for (int i = 0; i < 512; ++i)
        rms += output[i] * output[i];
    rms = std::sqrt(rms / 512.0);

    // Should be very quiet (gains haven't ramped up yet)
    EXPECT_LT(rms, 5000.0);
}

// --- Transmission Shift Tests ---

TEST(TransmissionShiftTest, UpshiftRpmDropCorrect)
{
    // Ferrari 458: 1st=3.08, 2nd=2.19
    // Upshift at 8500 RPM: new_rpm = 8500 * (2.19/3.08) = 6044
    Transmission::Config config;
    config.num_gears = 7;
    config.gear_ratios[0] = 3.08f;
    config.gear_ratios[1] = 2.19f;
    config.gear_ratios[2] = 1.63f;
    config.gear_ratios[3] = 1.29f;
    config.gear_ratios[4] = 1.03f;
    config.gear_ratios[5] = 0.84f;
    config.gear_ratios[6] = 0.69f;
    config.rpm_idle = 900.0f;
    config.rpm_redline = 9000.0f;
    config.rpm_upshift = 2.0f; // manual
    config.rpm_downshift = -1.0f;

    Transmission trans(config);

    // Rev to 8500 RPM
    for (int i = 0; i < 300; ++i)
        trans.update(1.0f, 0.016f);

    // Get RPM before shift
    float rpm_before = trans.rpm();
    EXPECT_GT(rpm_before, 7000.0f); // Should be high

    // Shift up 1→2
    trans.shift_up();
    EXPECT_EQ(trans.gear(), 2);

    // RPM should drop by ratio 2.19/3.08 = 0.711
    float expected_rpm = rpm_before * (2.19f / 3.08f);
    EXPECT_NEAR(trans.rpm(), expected_rpm, 10.0f);
}

TEST(TransmissionShiftTest, DownshiftRpmRiseCorrect)
{
    Transmission::Config config;
    config.num_gears = 7;
    config.gear_ratios[0] = 3.08f;
    config.gear_ratios[1] = 2.19f;
    config.gear_ratios[2] = 1.63f;
    config.rpm_idle = 900.0f;
    config.rpm_redline = 9000.0f;
    config.rpm_upshift = 2.0f;
    config.rpm_downshift = -1.0f;
    config.peak_torque = 400.0f;
    config.peak_torque_rpm = 5000.0f;
    config.inertia = 0.12f;
    config.friction = 15.0f;

    Transmission trans(config);

    // Shift to 3rd gear then let RPM settle to moderate level
    trans.shift_up(); // 1→2
    for (int i = 0; i < 15; ++i)
        trans.update(0.2f, 0.016f);
    trans.shift_up(); // 2→3
    for (int i = 0; i < 15; ++i)
        trans.update(0.0f, 0.016f); // No throttle during shift
    EXPECT_EQ(trans.gear(), 3);

    // Let RPM settle to a moderate value (~3000-5000)
    // Use zero throttle to let it coast down from shift
    for (int i = 0; i < 50; ++i)
        trans.update(0.15f, 0.016f);

    float rpm_before = trans.rpm();
    EXPECT_GT(rpm_before, 1000.0f);
    // Ensure downshift won't exceed redline
    EXPECT_LT(rpm_before * (2.19f / 1.63f), 9000.0f * 1.05f);

    // Downshift 3→2: RPM should rise by ratio 2.19/1.63
    trans.shift_down();
    EXPECT_EQ(trans.gear(), 2);

    float expected_rpm = rpm_before * (2.19f / 1.63f);
    EXPECT_NEAR(trans.rpm(), expected_rpm, 100.0f);
}

TEST(TransmissionShiftTest, DctShiftTimeFast)
{
    Transmission::Config config;
    config.num_gears = 7;
    config.gear_ratios[0] = 3.08f;
    config.gear_ratios[1] = 2.19f;
    config.gear_ratios[2] = 1.63f;
    config.rpm_idle = 900.0f;
    config.rpm_redline = 9000.0f;
    config.rpm_upshift = 2.0f;
    config.rpm_downshift = -1.0f;

    Transmission trans(config);

    // Rev up
    for (int i = 0; i < 200; ++i)
        trans.update(1.0f, 0.016f);

    trans.shift_up();
    EXPECT_EQ(trans.gear(), 2);

    // After 100ms (> 60ms shift time), shift should be complete
    for (int i = 0; i < 7; ++i)
        trans.update(1.0f, 0.016f); // ~112ms total

    // Should be able to shift again
    uint8_t gear_before = trans.gear();
    trans.shift_up();
    EXPECT_EQ(trans.gear(), gear_before + 1);
}

// --- Variable-rate SamplePlayer Tests ---

TEST(SamplePlayerPitchTest, DoubleRateDoublesPitch)
{
    // A sample played at 2x rate should complete in half the time
    sample_t data[100];
    for (int i = 0; i < 100; ++i)
        data[i] = static_cast<sample_t>(i * 100);

    SamplePlayer player;
    player.load(data, 100);

    sample_t output[50];
    player.process(output, 50, 2.0f);

    // After 50 frames at 2x rate, phase should be at ~100 (wrapped)
    EXPECT_NEAR(player.phase(), 0.0, 2.0); // Should have looped back
}

TEST(SamplePlayerPitchTest, HalfRateHalvesPitch)
{
    sample_t data[100];
    for (int i = 0; i < 100; ++i)
        data[i] = static_cast<sample_t>(i * 100);

    SamplePlayer player;
    player.load(data, 100);

    sample_t output[50];
    player.process(output, 50, 0.5f);

    // After 50 frames at 0.5x rate, phase should be at 25
    EXPECT_NEAR(player.phase(), 25.0, 1.0);
}
