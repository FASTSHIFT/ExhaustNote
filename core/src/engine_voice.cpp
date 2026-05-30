#include "core/engine_voice.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace exhaust {

void EngineVoice::set_onload_layers(const LayerConfig* layers, uint8_t num_layers)
{
    num_onload_ = std::min(static_cast<uint8_t>(kMaxLayers), num_layers);
    for (uint8_t i = 0; i < num_onload_; ++i) {
        onload_rpms_[i] = layers[i].rpm;
        onload_players_[i].load(layers[i].data, layers[i].length);
        onload_gains_[i] = 0.0f;
        onload_target_gains_[i] = 0.0f;
    }
}

void EngineVoice::set_offload_layers(const LayerConfig* layers, uint8_t num_layers)
{
    num_offload_ = std::min(static_cast<uint8_t>(kMaxLayers), num_layers);
    for (uint8_t i = 0; i < num_offload_; ++i) {
        offload_rpms_[i] = layers[i].rpm;
        offload_players_[i].load(layers[i].data, layers[i].length);
        offload_gains_[i] = 0.0f;
        offload_target_gains_[i] = 0.0f;
    }
}

void EngineVoice::set_engine_config(uint8_t cylinders, uint32_t sample_rate)
{
    cylinders_ = cylinders;
    sample_rate_ = sample_rate;
}

void EngineVoice::compute_layer_gains(float rpm, const float* layer_rpms,
    uint8_t num_layers, float* gains_out)
{
    if (num_layers == 0)
        return;

    // Compute Gaussian-shaped gains centered on each layer's RPM.
    // The width (sigma) is proportional to the distance to neighboring layers.
    float sum_sq = 0.0f;

    for (uint8_t i = 0; i < num_layers; ++i) {
        // Adaptive sigma: half the distance to nearest neighbor
        float sigma;
        if (num_layers == 1) {
            sigma = 1000.0f;
        } else if (i == 0) {
            sigma = (layer_rpms[1] - layer_rpms[0]) * 0.6f;
        } else if (i == num_layers - 1) {
            sigma = (layer_rpms[i] - layer_rpms[i - 1]) * 0.6f;
        } else {
            sigma = std::fmin(layer_rpms[i] - layer_rpms[i - 1],
                        layer_rpms[i + 1] - layer_rpms[i])
                * 0.6f;
        }

        if (sigma < 100.0f)
            sigma = 100.0f;

        float diff = (rpm - layer_rpms[i]) / sigma;
        float gain = std::exp(-0.5f * diff * diff);
        gains_out[i] = gain;
        sum_sq += gain * gain;
    }

    // Equal-power normalization: scale so sum of squares = 1
    if (sum_sq > 1e-8f) {
        float scale = 1.0f / std::sqrt(sum_sq);
        for (uint8_t i = 0; i < num_layers; ++i) {
            gains_out[i] *= scale;
        }
    }
}

void EngineVoice::smooth_gains(float* target, float* current, uint8_t count, float alpha)
{
    for (uint8_t i = 0; i < count; ++i) {
        current[i] += alpha * (target[i] - current[i]);
    }
}

void EngineVoice::process(const Params& params, sample_t* output, size_t frames)
{
    frames = std::min(frames, static_cast<size_t>(kDefaultBufferFrames));

    // Smoothing alpha: 1 - exp(-dt/tc), where dt = frames/sample_rate
    float dt = static_cast<float>(frames) / static_cast<float>(sample_rate_);
    float alpha = 1.0f - std::exp(-dt / gain_smooth_tc_);

    // --- Compute target gains for all layers ---
    compute_layer_gains(params.rpm, onload_rpms_, num_onload_, onload_target_gains_);
    compute_layer_gains(params.rpm, offload_rpms_, num_offload_, offload_target_gains_);

    // --- Smooth gains ---
    smooth_gains(onload_target_gains_, onload_gains_, num_onload_, alpha);
    smooth_gains(offload_target_gains_, offload_gains_, num_offload_, alpha);

    // --- Mix all onload layers (all always playing, pitch-shifted) ---
    std::memset(buf_onload_f_, 0, frames * sizeof(float));
    for (uint8_t i = 0; i < num_onload_; ++i) {
        // Playback rate = current_rpm / layer_recorded_rpm
        // This pitch-shifts the sample to match the current RPM
        float rate = params.rpm / onload_rpms_[i];
        // Clamp rate to avoid extreme pitch shifts (max 2x up/down)
        rate = std::fmax(0.5f, std::fmin(2.0f, rate));

        if (onload_gains_[i] < 0.001f) {
            // Still advance phase even if silent (keeps layers ready)
            onload_players_[i].process(buf_layer_, frames, rate);
            continue;
        }
        onload_players_[i].process(buf_layer_, frames, rate);
        float g = onload_gains_[i];
        for (size_t s = 0; s < frames; ++s) {
            buf_onload_f_[s] += static_cast<float>(buf_layer_[s]) * g;
        }
    }

    // --- Mix all offload layers (pitch-shifted) ---
    std::memset(buf_offload_f_, 0, frames * sizeof(float));
    for (uint8_t i = 0; i < num_offload_; ++i) {
        float rate = params.rpm / offload_rpms_[i];
        rate = std::fmax(0.5f, std::fmin(2.0f, rate));

        if (offload_gains_[i] < 0.001f) {
            offload_players_[i].process(buf_layer_, frames, rate);
            continue;
        }
        offload_players_[i].process(buf_layer_, frames, rate);
        float g = offload_gains_[i];
        for (size_t s = 0; s < frames; ++s) {
            buf_offload_f_[s] += static_cast<float>(buf_layer_[s]) * g;
        }
    }

    // --- Blend onload/offload by throttle (equal-power) ---
    float throttle = std::fmax(0.0f, std::fmin(1.0f, params.throttle));
    // Equal-power: cos/sin panning
    float on_gain = std::sin(throttle * 1.5707963f); // pi/2
    float off_gain = std::cos(throttle * 1.5707963f);

    for (size_t s = 0; s < frames; ++s) {
        float sample = buf_onload_f_[s] * on_gain + buf_offload_f_[s] * off_gain;
        // Soft clamp
        sample = std::fmax(-32768.0f, std::fmin(32767.0f, sample));
        output[s] = static_cast<sample_t>(sample);
    }

    // --- Apply EQ tracking engine fundamental frequency ---
    float fundamental = params.rpm * static_cast<float>(cylinders_) / 120.0f;
    if (fundamental > 20.0f && fundamental < static_cast<float>(sample_rate_) / 2.0f) {
        eq_.set_peaking_eq(static_cast<float>(sample_rate_), fundamental, 2.0f, 2.0f);
        eq_.process(output, frames);
    }
}

void EngineVoice::reset()
{
    for (uint8_t i = 0; i < kMaxLayers; ++i) {
        onload_players_[i].reset();
        offload_players_[i].reset();
        onload_gains_[i] = 0.0f;
        offload_gains_[i] = 0.0f;
    }
    eq_.reset();
}

} // namespace exhaust
