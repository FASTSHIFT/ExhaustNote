#include "core/crossfade.h"

#include <gtest/gtest.h>

using namespace exhaust;

TEST(CrossfadeTest, SingleLayer)
{
    float rpms[] = { 1000.0f };
    auto result = compute_crossfade(1000.0f, rpms, 1);
    EXPECT_EQ(result.layer_lo, 0);
    EXPECT_EQ(result.layer_hi, 0);
    EXPECT_FLOAT_EQ(result.mix, 0.0f);
}

TEST(CrossfadeTest, BelowMinRpm)
{
    float rpms[] = { 1000.0f, 3000.0f, 5000.0f };
    auto result = compute_crossfade(500.0f, rpms, 3);
    EXPECT_EQ(result.layer_lo, 0);
    EXPECT_EQ(result.layer_hi, 0);
    EXPECT_FLOAT_EQ(result.mix, 0.0f);
}

TEST(CrossfadeTest, AboveMaxRpm)
{
    float rpms[] = { 1000.0f, 3000.0f, 5000.0f };
    auto result = compute_crossfade(7000.0f, rpms, 3);
    EXPECT_EQ(result.layer_lo, 2);
    EXPECT_EQ(result.layer_hi, 2);
    EXPECT_FLOAT_EQ(result.mix, 0.0f);
}

TEST(CrossfadeTest, ExactlyOnLayer)
{
    float rpms[] = { 1000.0f, 3000.0f, 5000.0f };
    auto result = compute_crossfade(1000.0f, rpms, 3);
    EXPECT_EQ(result.layer_lo, 0);
    // At exact boundary, mix should be 0
    EXPECT_FLOAT_EQ(result.mix, 0.0f);
}

TEST(CrossfadeTest, MidpointBetweenLayers)
{
    float rpms[] = { 1000.0f, 3000.0f, 5000.0f };
    auto result = compute_crossfade(2000.0f, rpms, 3);
    EXPECT_EQ(result.layer_lo, 0);
    EXPECT_EQ(result.layer_hi, 1);
    EXPECT_FLOAT_EQ(result.mix, 0.5f);
}

TEST(CrossfadeTest, QuarterPoint)
{
    float rpms[] = { 1000.0f, 5000.0f };
    auto result = compute_crossfade(2000.0f, rpms, 2);
    EXPECT_EQ(result.layer_lo, 0);
    EXPECT_EQ(result.layer_hi, 1);
    EXPECT_FLOAT_EQ(result.mix, 0.25f);
}

TEST(CrossfadeTest, ThreeQuarterPoint)
{
    float rpms[] = { 1000.0f, 5000.0f };
    auto result = compute_crossfade(4000.0f, rpms, 2);
    EXPECT_EQ(result.layer_lo, 0);
    EXPECT_EQ(result.layer_hi, 1);
    EXPECT_FLOAT_EQ(result.mix, 0.75f);
}

TEST(CrossfadeTest, ZeroLayers)
{
    auto result = compute_crossfade(1000.0f, nullptr, 0);
    EXPECT_EQ(result.layer_lo, 0);
    EXPECT_EQ(result.layer_hi, 0);
    EXPECT_FLOAT_EQ(result.mix, 0.0f);
}

TEST(MixLayersTest, FullLow)
{
    sample_t lo[] = { 1000, 2000, 3000 };
    sample_t hi[] = { -1000, -2000, -3000 };
    sample_t output[3];

    mix_layers(lo, hi, output, 3, 0.0f);
    EXPECT_EQ(output[0], 1000);
    EXPECT_EQ(output[1], 2000);
    EXPECT_EQ(output[2], 3000);
}

TEST(MixLayersTest, FullHigh)
{
    sample_t lo[] = { 1000, 2000, 3000 };
    sample_t hi[] = { -1000, -2000, -3000 };
    sample_t output[3];

    mix_layers(lo, hi, output, 3, 1.0f);
    EXPECT_EQ(output[0], -1000);
    EXPECT_EQ(output[1], -2000);
    EXPECT_EQ(output[2], -3000);
}

TEST(MixLayersTest, HalfMix)
{
    sample_t lo[] = { 1000, 0, -1000 };
    sample_t hi[] = { -1000, 0, 1000 };
    sample_t output[3];

    mix_layers(lo, hi, output, 3, 0.5f);
    EXPECT_EQ(output[0], 0);
    EXPECT_EQ(output[1], 0);
    EXPECT_EQ(output[2], 0);
}

TEST(MixLayersTest, ClampOverflow)
{
    sample_t lo[] = { 32000 };
    sample_t hi[] = { 32000 };
    sample_t output[1];

    mix_layers(lo, hi, output, 1, 0.5f);
    // Should not exceed 32767
    EXPECT_LE(output[0], 32767);
    EXPECT_GE(output[0], -32768);
}
