#include "core/crossfade.h"

#include <algorithm>
#include <cmath>

namespace exhaust {

CrossfadeResult compute_crossfade(float rpm, const float* layer_rpms, uint8_t num_layers)
{
    CrossfadeResult result = {};

    if (num_layers == 0) {
        return result;
    }

    if (num_layers == 1) {
        result.layer_lo = 0;
        result.layer_hi = 0;
        result.mix = 0.0f;
        return result;
    }

    // Clamp RPM to layer range
    if (rpm <= layer_rpms[0]) {
        result.layer_lo = 0;
        result.layer_hi = 0;
        result.mix = 0.0f;
        return result;
    }

    if (rpm >= layer_rpms[num_layers - 1]) {
        result.layer_lo = num_layers - 1;
        result.layer_hi = num_layers - 1;
        result.mix = 0.0f;
        return result;
    }

    // Find the two adjacent layers
    for (uint8_t i = 0; i < num_layers - 1; ++i) {
        if (rpm >= layer_rpms[i] && rpm < layer_rpms[i + 1]) {
            result.layer_lo = i;
            result.layer_hi = i + 1;
            float range = layer_rpms[i + 1] - layer_rpms[i];
            result.mix = (range > 0.0f) ? (rpm - layer_rpms[i]) / range : 0.0f;
            return result;
        }
    }

    // Fallback (should not reach here)
    result.layer_lo = num_layers - 1;
    result.layer_hi = num_layers - 1;
    result.mix = 0.0f;
    return result;
}

void mix_layers(const sample_t* lo, const sample_t* hi, sample_t* output,
    size_t frames, float mix)
{
    float inv_mix = 1.0f - mix;
    for (size_t i = 0; i < frames; ++i) {
        float sample = static_cast<float>(lo[i]) * inv_mix + static_cast<float>(hi[i]) * mix;
        // Clamp to int16 range
        sample = std::fmax(-32768.0f, std::fmin(32767.0f, sample));
        output[i] = static_cast<sample_t>(sample);
    }
}

} // namespace exhaust
