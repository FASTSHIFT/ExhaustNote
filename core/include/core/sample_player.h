#pragma once

#include "core/types.h"

namespace exhaust {

/// A looping sample player that reads from a pre-loaded PCM buffer.
/// Supports fractional-sample phase accumulation for pitch-invariant playback.
class SamplePlayer {
public:
    SamplePlayer() = default;

    /// Load a sample buffer (does not take ownership).
    /// @param data       Pointer to 16-bit PCM sample data
    /// @param length     Number of samples in the buffer
    /// @param loop_start Start of loop region (in samples)
    /// @param loop_end   End of loop region (in samples, exclusive)
    void load(const sample_t* data, size_t length, size_t loop_start, size_t loop_end);

    /// Convenience: load with loop over entire buffer.
    void load(const sample_t* data, size_t length);

    /// Reset playback position to the beginning.
    void reset();

    /// Generate samples at 1:1 playback rate (no pitch shift).
    /// @param output  Output buffer to fill
    /// @param frames  Number of frames to generate
    void process(sample_t* output, size_t frames);

    /// Generate samples at variable playback rate (pitch shift).
    /// @param output  Output buffer to fill
    /// @param frames  Number of frames to generate
    /// @param rate    Playback rate (1.0 = original, 2.0 = octave up, 0.5 = octave down)
    void process(sample_t* output, size_t frames, float rate);

    /// Generate samples into a float buffer at variable rate.
    /// @param output  Float output buffer
    /// @param frames  Number of frames to generate
    /// @param rate    Playback rate
    void process_float(float* output, size_t frames, float rate);

    /// Check if a sample is loaded.
    bool is_loaded() const { return data_ != nullptr; }

    /// Get current playback phase position.
    float phase() const { return phase_; }

private:
    const sample_t* data_ = nullptr;
    size_t length_ = 0;
    size_t loop_start_ = 0;
    size_t loop_end_ = 0;
    float phase_ = 0.0f;
};

} // namespace exhaust
