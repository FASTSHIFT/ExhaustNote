#include "core/dsp.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace exhaust {

void BiquadFilter::set_peaking_eq(float sample_rate, float freq, float q, float gain_db)
{
    float A = std::pow(10.0f, gain_db / 40.0f);
    float w0 = 2.0f * static_cast<float>(M_PI) * freq / sample_rate;
    float sin_w0 = std::sin(w0);
    float cos_w0 = std::cos(w0);
    float alpha = sin_w0 / (2.0f * q);

    float a0 = 1.0f + alpha / A;
    b0_ = (1.0f + alpha * A) / a0;
    b1_ = (-2.0f * cos_w0) / a0;
    b2_ = (1.0f - alpha * A) / a0;
    a1_ = (-2.0f * cos_w0) / a0;
    a2_ = (1.0f - alpha / A) / a0;

    reset();
}

void BiquadFilter::set_lowpass(float sample_rate, float freq, float q)
{
    float w0 = 2.0f * static_cast<float>(M_PI) * freq / sample_rate;
    float sin_w0 = std::sin(w0);
    float cos_w0 = std::cos(w0);
    float alpha = sin_w0 / (2.0f * q);

    float a0 = 1.0f + alpha;
    b0_ = ((1.0f - cos_w0) / 2.0f) / a0;
    b1_ = (1.0f - cos_w0) / a0;
    b2_ = ((1.0f - cos_w0) / 2.0f) / a0;
    a1_ = (-2.0f * cos_w0) / a0;
    a2_ = (1.0f - alpha) / a0;

    reset();
}

void BiquadFilter::set_highpass(float sample_rate, float freq, float q)
{
    float w0 = 2.0f * static_cast<float>(M_PI) * freq / sample_rate;
    float sin_w0 = std::sin(w0);
    float cos_w0 = std::cos(w0);
    float alpha = sin_w0 / (2.0f * q);

    float a0 = 1.0f + alpha;
    b0_ = ((1.0f + cos_w0) / 2.0f) / a0;
    b1_ = (-(1.0f + cos_w0)) / a0;
    b2_ = ((1.0f + cos_w0) / 2.0f) / a0;
    a1_ = (-2.0f * cos_w0) / a0;
    a2_ = (1.0f - alpha) / a0;

    reset();
}

void BiquadFilter::process(sample_t* samples, size_t frames)
{
    for (size_t i = 0; i < frames; ++i) {
        samples[i] = process_sample(samples[i]);
    }
}

sample_t BiquadFilter::process_sample(sample_t input)
{
    float x = static_cast<float>(input);
    float y = b0_ * x + z1_;
    z1_ = b1_ * x - a1_ * y + z2_;
    z2_ = b2_ * x - a2_ * y;

    // Clamp output
    y = std::fmax(-32768.0f, std::fmin(32767.0f, y));
    return static_cast<sample_t>(y);
}

void BiquadFilter::reset()
{
    z1_ = 0.0f;
    z2_ = 0.0f;
}

} // namespace exhaust
