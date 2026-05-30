#include "core/mixer.h"

#include <cstring>
#include <gtest/gtest.h>

using namespace exhaust;

TEST(MixerTest, ClearProducesSilence)
{
    Mixer mixer;
    mixer.clear(64);

    sample_t output[64];
    mixer.finalize(output, 64);

    for (size_t i = 0; i < 64; ++i) {
        EXPECT_EQ(output[i], 0);
    }
}

TEST(MixerTest, SingleChannelUnityGain)
{
    Mixer mixer;
    mixer.set_master_volume(1.0f);
    mixer.clear(4);

    sample_t input[] = { 1000, 2000, 3000, 4000 };
    mixer.add(input, 4, 1.0f);

    sample_t output[4];
    mixer.finalize(output, 4);

    EXPECT_EQ(output[0], 1000);
    EXPECT_EQ(output[1], 2000);
    EXPECT_EQ(output[2], 3000);
    EXPECT_EQ(output[3], 4000);
}

TEST(MixerTest, TwoChannelsMix)
{
    Mixer mixer;
    mixer.set_master_volume(1.0f);
    mixer.clear(3);

    sample_t ch1[] = { 1000, 2000, 3000 };
    sample_t ch2[] = { 500, 1000, 1500 };
    mixer.add(ch1, 3, 1.0f);
    mixer.add(ch2, 3, 1.0f);

    sample_t output[3];
    mixer.finalize(output, 3);

    EXPECT_EQ(output[0], 1500);
    EXPECT_EQ(output[1], 3000);
    EXPECT_EQ(output[2], 4500);
}

TEST(MixerTest, GainScaling)
{
    Mixer mixer;
    mixer.set_master_volume(1.0f);
    mixer.clear(2);

    sample_t input[] = { 10000, 20000 };
    mixer.add(input, 2, 0.5f);

    sample_t output[2];
    mixer.finalize(output, 2);

    EXPECT_EQ(output[0], 5000);
    EXPECT_EQ(output[1], 10000);
}

TEST(MixerTest, MasterVolume)
{
    Mixer mixer;
    mixer.set_master_volume(0.5f);
    mixer.clear(2);

    sample_t input[] = { 10000, 20000 };
    mixer.add(input, 2, 1.0f);

    sample_t output[2];
    mixer.finalize(output, 2);

    EXPECT_EQ(output[0], 5000);
    EXPECT_EQ(output[1], 10000);
}

TEST(MixerTest, SoftClipPreventsOverflow)
{
    Mixer mixer;
    mixer.set_master_volume(1.0f);
    mixer.clear(1);

    // Add multiple loud channels to cause overflow
    sample_t loud[] = { 30000 };
    mixer.add(loud, 1, 1.0f);
    mixer.add(loud, 1, 1.0f);
    mixer.add(loud, 1, 1.0f); // Total: 90000

    sample_t output[1];
    mixer.finalize(output, 1);

    // Should be clipped to valid range
    EXPECT_LE(output[0], 32767);
    EXPECT_GE(output[0], -32768);
    // Should still be positive and loud (soft clip, not hard zero)
    EXPECT_GT(output[0], 20000);
}

TEST(MixerTest, NegativeOverflowClipped)
{
    Mixer mixer;
    mixer.set_master_volume(1.0f);
    mixer.clear(1);

    sample_t loud[] = { -30000 };
    mixer.add(loud, 1, 1.0f);
    mixer.add(loud, 1, 1.0f);
    mixer.add(loud, 1, 1.0f);

    sample_t output[1];
    mixer.finalize(output, 1);

    EXPECT_LE(output[0], 32767);
    EXPECT_GE(output[0], -32768);
    EXPECT_LT(output[0], -20000);
}

TEST(MixerTest, SetAndGetMasterVolume)
{
    Mixer mixer;
    mixer.set_master_volume(0.42f);
    EXPECT_FLOAT_EQ(mixer.master_volume(), 0.42f);
}
