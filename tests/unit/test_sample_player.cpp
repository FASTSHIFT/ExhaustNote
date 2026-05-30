#include "core/sample_player.h"

#include <cstring>
#include <gtest/gtest.h>
#include <vector>

using namespace exhaust;

TEST(SamplePlayerTest, UnloadedProducesSilence)
{
    SamplePlayer player;
    sample_t output[64];
    player.process(output, 64);

    for (size_t i = 0; i < 64; ++i) {
        EXPECT_EQ(output[i], 0);
    }
}

TEST(SamplePlayerTest, LoadAndPlay)
{
    sample_t data[] = { 100, 200, 300, 400, 500 };
    SamplePlayer player;
    player.load(data, 5);

    sample_t output[5];
    player.process(output, 5);

    // First pass should reproduce the data (with interpolation at boundaries)
    EXPECT_EQ(output[0], 100);
    EXPECT_EQ(output[1], 200);
    EXPECT_EQ(output[2], 300);
    EXPECT_EQ(output[3], 400);
    // output[4] interpolates between data[4] and data[0] (loop)
}

TEST(SamplePlayerTest, LoopsCorrectly)
{
    sample_t data[] = { 1000, 2000, 3000 };
    SamplePlayer player;
    player.load(data, 3);

    // Play more than one loop
    sample_t output[9];
    player.process(output, 9);

    // After 3 samples, should loop back
    EXPECT_EQ(output[0], 1000);
    EXPECT_EQ(output[1], 2000);
    // output[2] interpolates data[2] with data[0] (next after loop)
    // After loop: output[3] should be near data[0] again
    EXPECT_EQ(output[3], 1000);
    EXPECT_EQ(output[4], 2000);
}

TEST(SamplePlayerTest, ResetGoesBackToStart)
{
    sample_t data[] = { 100, 200, 300, 400 };
    SamplePlayer player;
    player.load(data, 4);

    sample_t output[2];
    player.process(output, 2);
    EXPECT_EQ(output[0], 100);

    player.reset();
    EXPECT_DOUBLE_EQ(player.phase(), 0.0);

    player.process(output, 1);
    EXPECT_EQ(output[0], 100);
}

TEST(SamplePlayerTest, IsLoadedCheck)
{
    SamplePlayer player;
    EXPECT_FALSE(player.is_loaded());

    sample_t data[] = { 0 };
    player.load(data, 1);
    EXPECT_TRUE(player.is_loaded());
}

TEST(SamplePlayerTest, CustomLoopRegion)
{
    // Data: [0, 100, 200, 300, 400]
    // Loop region: [2, 4) → loops over {200, 300}
    sample_t data[] = { 0, 100, 200, 300, 400 };
    SamplePlayer player;
    player.load(data, 5, 2, 4);

    // First two samples are before loop region (play-through)
    sample_t output[8];
    player.process(output, 8);

    // Starts at phase 0, plays 0, 100, then enters loop at 200, 300, 200, 300...
    EXPECT_EQ(output[0], 0);
    EXPECT_EQ(output[1], 100);
    EXPECT_EQ(output[2], 200);
    // output[3] interpolates 300 with 200 (loop back)
}

TEST(SamplePlayerTest, ZeroLengthProducesSilence)
{
    sample_t data[] = { 1000 };
    SamplePlayer player;
    player.load(data, 0);

    sample_t output[4];
    player.process(output, 4);
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(output[i], 0);
    }
}
