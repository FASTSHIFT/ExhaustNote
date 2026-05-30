#pragma once

#include "core/types.h"

namespace exhaust {

/// Second-order IIR (biquad) filter.
/// Implements Direct Form II Transposed.
class BiquadFilter {
public:
    BiquadFilter() = default;

    /// Configure as a peaking EQ filter.
    /// @param sample_rate  Audio sample rate in Hz
    /// @param freq         Center frequency in Hz
    /// @param q            Q factor (bandwidth)
    /// @param gain_db      Gain at center frequency in dB
    void set_peaking_eq(float sample_rate, float freq, float q, float gain_db);

    /// Configure as a low-pass filter.
    /// @param sample_rate  Audio sample rate in Hz
    /// @param freq         Cutoff frequency in Hz
    /// @param q            Q factor
    void set_lowpass(float sample_rate, float freq, float q);

    /// Configure as a high-pass filter.
    /// @param sample_rate  Audio sample rate in Hz
    /// @param freq         Cutoff frequency in Hz
    /// @param q            Q factor
    void set_highpass(float sample_rate, float freq, float q);

    /// Process a buffer of samples in-place.
    /// @param samples  Buffer to process
    /// @param frames   Number of frames
    void process(sample_t* samples, size_t frames);

    /// Process a single sample and return the filtered value.
    sample_t process_sample(sample_t input);

    /// Reset filter state (clear delay line).
    void reset();

private:
    // Coefficients
    float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f;
    float a1_ = 0.0f, a2_ = 0.0f;
    // State (Direct Form II Transposed)
    float z1_ = 0.0f, z2_ = 0.0f;
};

} // namespace exhaust
