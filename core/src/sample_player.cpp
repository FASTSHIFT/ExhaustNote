#include "core/sample_player.h"

#include <cstring>

namespace exhaust {

void SamplePlayer::load(const sample_t* data, size_t length, size_t loop_start, size_t loop_end)
{
    data_ = data;
    length_ = length;
    loop_start_ = loop_start;
    loop_end_ = (loop_end > 0) ? loop_end : length;
    phase_ = 0.0f;
}

void SamplePlayer::load(const sample_t* data, size_t length)
{
    load(data, length, 0, length);
}

void SamplePlayer::reset()
{
    phase_ = 0.0f;
}

void SamplePlayer::process(sample_t* output, size_t frames)
{
    process(output, frames, 1.0f);
}

void SamplePlayer::process(sample_t* output, size_t frames, float rate)
{
    if (!data_ || length_ == 0) {
        std::memset(output, 0, frames * sizeof(sample_t));
        return;
    }

    size_t loop_len = loop_end_ - loop_start_;
    if (loop_len == 0) {
        std::memset(output, 0, frames * sizeof(sample_t));
        return;
    }

    for (size_t i = 0; i < frames; ++i) {
        // Integer index with linear interpolation
        size_t idx = static_cast<size_t>(phase_);
        float frac = phase_ - static_cast<float>(idx);

        size_t idx_next = idx + 1;
        if (idx_next >= loop_end_) {
            idx_next = loop_start_;
        }

        // Linear interpolation between adjacent samples
        float s0 = static_cast<float>(data_[idx]);
        float s1 = static_cast<float>(data_[idx_next]);
        float sample = s0 + frac * (s1 - s0);

        output[i] = static_cast<sample_t>(sample);

        // Advance phase at variable rate
        phase_ += rate;
        if (phase_ >= static_cast<float>(loop_end_)) {
            phase_ = static_cast<float>(loop_start_) + (phase_ - static_cast<float>(loop_end_));
        } else if (phase_ < 0.0f) {
            phase_ = 0.0f;
        }
    }
}

void SamplePlayer::process_float(float* output, size_t frames, float rate)
{
    if (!data_ || length_ == 0) {
        std::memset(output, 0, frames * sizeof(float));
        return;
    }

    size_t loop_len = loop_end_ - loop_start_;
    if (loop_len == 0) {
        std::memset(output, 0, frames * sizeof(float));
        return;
    }

    for (size_t i = 0; i < frames; ++i) {
        size_t idx = static_cast<size_t>(phase_);
        float frac = phase_ - static_cast<float>(idx);

        size_t idx_next = idx + 1;
        if (idx_next >= loop_end_) {
            idx_next = loop_start_;
        }

        float s0 = static_cast<float>(data_[idx]);
        float s1 = static_cast<float>(data_[idx_next]);
        output[i] = s0 + frac * (s1 - s0);

        phase_ += rate;
        if (phase_ >= static_cast<float>(loop_end_)) {
            phase_ = static_cast<float>(loop_start_) + (phase_ - static_cast<float>(loop_end_));
        } else if (phase_ < 0.0f) {
            phase_ = 0.0f;
        }
    }
}

} // namespace exhaust
