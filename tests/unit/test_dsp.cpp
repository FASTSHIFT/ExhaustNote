#include "core/dsp.h"

#include <cmath>
#include <gtest/gtest.h>
#include <vector>

using namespace exhaust;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper: generate a sine wave
static std::vector<sample_t> generate_sine(float freq, float sample_rate, size_t length, float amplitude = 16000.0f)
{
    std::vector<sample_t> buf(length);
    for (size_t i = 0; i < length; ++i) {
        float t = static_cast<float>(i) / sample_rate;
        buf[i] = static_cast<sample_t>(amplitude * std::sin(2.0f * static_cast<float>(M_PI) * freq * t));
    }
    return buf;
}

// Helper: compute RMS of a buffer
static float compute_rms(const sample_t* data, size_t length)
{
    double sum = 0.0;
    for (size_t i = 0; i < length; ++i) {
        sum += static_cast<double>(data[i]) * static_cast<double>(data[i]);
    }
    return static_cast<float>(std::sqrt(sum / static_cast<double>(length)));
}

TEST(BiquadFilterTest, PassthroughWhenFlat)
{
    BiquadFilter filter;
    // Default coefficients: b0=1, others=0 → passthrough
    sample_t data[] = { 1000, -2000, 3000, -4000 };
    filter.process(data, 4);
    EXPECT_EQ(data[0], 1000);
    EXPECT_EQ(data[1], -2000);
    EXPECT_EQ(data[2], 3000);
    EXPECT_EQ(data[3], -4000);
}

TEST(BiquadFilterTest, LowpassAttenuatesHighFreq)
{
    BiquadFilter filter;
    filter.set_lowpass(44100.0f, 200.0f, 0.707f);

    // Generate a high-frequency signal (5000 Hz)
    auto signal = generate_sine(5000.0f, 44100.0f, 4096);
    float rms_before = compute_rms(signal.data(), signal.size());

    filter.process(signal.data(), signal.size());
    float rms_after = compute_rms(signal.data(), signal.size());

    // High freq should be significantly attenuated
    EXPECT_LT(rms_after, rms_before * 0.1f);
}

TEST(BiquadFilterTest, LowpassPassesLowFreq)
{
    BiquadFilter filter;
    filter.set_lowpass(44100.0f, 5000.0f, 0.707f);

    // Generate a low-frequency signal (100 Hz)
    auto signal = generate_sine(100.0f, 44100.0f, 4096);
    float rms_before = compute_rms(signal.data(), signal.size());

    filter.process(signal.data(), signal.size());
    float rms_after = compute_rms(signal.data(), signal.size());

    // Low freq should pass through mostly unchanged (within 10%)
    EXPECT_GT(rms_after, rms_before * 0.9f);
}

TEST(BiquadFilterTest, HighpassAttenuatesLowFreq)
{
    BiquadFilter filter;
    filter.set_highpass(44100.0f, 5000.0f, 0.707f);

    // Generate a low-frequency signal (100 Hz)
    auto signal = generate_sine(100.0f, 44100.0f, 4096);
    float rms_before = compute_rms(signal.data(), signal.size());

    filter.process(signal.data(), signal.size());
    float rms_after = compute_rms(signal.data(), signal.size());

    // Low freq should be significantly attenuated
    EXPECT_LT(rms_after, rms_before * 0.1f);
}

TEST(BiquadFilterTest, ResetClearsState)
{
    BiquadFilter filter;
    filter.set_lowpass(44100.0f, 1000.0f, 0.707f);

    // Process some samples to build up state
    sample_t data[] = { 10000, -10000, 10000, -10000 };
    filter.process(data, 4);

    filter.reset();

    // After reset, processing the same input should give same result as fresh
    BiquadFilter fresh;
    fresh.set_lowpass(44100.0f, 1000.0f, 0.707f);

    sample_t data1[] = { 5000 };
    sample_t data2[] = { 5000 };
    filter.process(data1, 1);
    fresh.process(data2, 1);

    EXPECT_EQ(data1[0], data2[0]);
}

TEST(BiquadFilterTest, PeakingEqBoostsAtCenter)
{
    BiquadFilter filter;
    filter.set_peaking_eq(44100.0f, 1000.0f, 1.0f, 12.0f);

    // Signal at center frequency
    auto signal_center = generate_sine(1000.0f, 44100.0f, 4096);
    float rms_center_before = compute_rms(signal_center.data(), signal_center.size());
    filter.process(signal_center.data(), signal_center.size());
    float rms_center_after = compute_rms(signal_center.data(), signal_center.size());

    // Should be boosted
    EXPECT_GT(rms_center_after, rms_center_before);
}

TEST(BiquadFilterTest, ProcessSingleSample)
{
    BiquadFilter filter;
    // Default passthrough
    sample_t result = filter.process_sample(12345);
    EXPECT_EQ(result, 12345);
}
