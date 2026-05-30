#include "core/mixer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace exhaust {

void Mixer::clear(size_t frames)
{
    size_t n = std::min(frames, kMaxFrames);
    std::memset(mix_buffer_, 0, n * sizeof(float));
}

void Mixer::add(const sample_t* input, size_t frames, float gain)
{
    size_t n = std::min(frames, kMaxFrames);
    for (size_t i = 0; i < n; ++i) {
        mix_buffer_[i] += static_cast<float>(input[i]) * gain;
    }
}

void Mixer::finalize(sample_t* output, size_t frames)
{
    size_t n = std::min(frames, kMaxFrames);
    for (size_t i = 0; i < n; ++i) {
        float sample = mix_buffer_[i] * master_volume_;

        // Soft clipping using tanh-like curve
        if (sample > 24000.0f || sample < -24000.0f) {
            // Soft saturation region
            float normalized = sample / 32768.0f;
            normalized = std::tanh(normalized);
            sample = normalized * 32768.0f;
        }

        // Hard clamp as safety net
        sample = std::fmax(-32768.0f, std::fmin(32767.0f, sample));
        output[i] = static_cast<sample_t>(sample);
    }
}

} // namespace exhaust
