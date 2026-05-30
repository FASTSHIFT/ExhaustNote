#include "core/engine_voice.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace exhaust {

// --- Static helpers ---

float EngineVoice::compute_trapezoid_gain(float rpm, const LayerConfig& layer)
{
    // Below fade-in start: silent
    if (rpm <= layer.vol_in_start)
        return 0.0f;
    // Fade-in region
    if (rpm < layer.vol_in_end) {
        float range = layer.vol_in_end - layer.vol_in_start;
        return (range > 0.0f) ? (rpm - layer.vol_in_start) / range : 1.0f;
    }
    // Hold region (full volume)
    if (rpm <= layer.vol_out_start)
        return 1.0f;
    // Fade-out region
    if (rpm < layer.vol_out_end) {
        float range = layer.vol_out_end - layer.vol_out_start;
        return (range > 0.0f) ? 1.0f - (rpm - layer.vol_out_start) / range : 0.0f;
    }
    // Above fade-out end: silent
    return 0.0f;
}

float EngineVoice::compute_layer_pitch(float rpm, const LayerConfig& layer)
{
    float range = layer.pitch_end_rpm - layer.pitch_start_rpm;
    if (range <= 0.0f)
        return 1.0f;

    float t = (rpm - layer.pitch_start_rpm) / range;
    t = std::fmax(0.0f, std::fmin(1.0f, t));

    float pitch = layer.min_pitch + t * (layer.max_pitch - layer.min_pitch);
    return std::fmax(0.25f, std::fmin(4.0f, pitch));
}

void EngineVoice::auto_fill_envelopes(LayerConfig* layers, uint8_t num_layers)
{
    if (num_layers == 0)
        return;

    for (uint8_t i = 0; i < num_layers; ++i) {
        float rpm = layers[i].rpm;

        float prev_rpm = (i > 0) ? layers[i - 1].rpm : rpm - 1000.0f;
        float next_rpm = (i < num_layers - 1) ? layers[i + 1].rpm : rpm + 1000.0f;

        // Midpoints between this layer and neighbors
        float mid_down = (i > 0) ? (prev_rpm + rpm) * 0.5f : 0.0f;
        float mid_up = (i < num_layers - 1) ? (rpm + next_rpm) * 0.5f : 99999.0f;

        // Full overlap: fade-in starts where previous layer's fade-out starts
        // and ends at the midpoint. This ensures sum of gains = 1.0 at midpoint.
        if (i == 0) {
            layers[i].vol_in_start = 0.0f;
            layers[i].vol_in_end = 0.0f;
        } else {
            // Fade in from previous layer's center to midpoint
            layers[i].vol_in_start = prev_rpm;
            layers[i].vol_in_end = mid_down;
        }

        if (i == num_layers - 1) {
            layers[i].vol_out_start = 99999.0f;
            layers[i].vol_out_end = 99999.0f;
        } else {
            // Fade out from midpoint to next layer's center
            layers[i].vol_out_start = mid_up;
            layers[i].vol_out_end = next_rpm;
        }

        // Pitch range: spans from previous midpoint to next midpoint
        float pitch_lo = (i > 0) ? mid_down : rpm * 0.5f;
        float pitch_hi = (i < num_layers - 1) ? mid_up : rpm * 1.5f;
        layers[i].pitch_start_rpm = pitch_lo;
        layers[i].pitch_end_rpm = pitch_hi;
        layers[i].min_pitch = pitch_lo / rpm;
        layers[i].max_pitch = pitch_hi / rpm;
    }
}

void EngineVoice::smooth_gains(float* target, float* current, uint8_t count, float alpha)
{
    for (uint8_t i = 0; i < count; ++i) {
        current[i] += alpha * (target[i] - current[i]);
    }
}

// --- Public API ---

void EngineVoice::set_onload_layers(const LayerConfig* layers, uint8_t num_layers)
{
    num_onload_ = std::min(static_cast<uint8_t>(kMaxLayers), num_layers);
    for (uint8_t i = 0; i < num_onload_; ++i) {
        onload_configs_[i] = layers[i];
        onload_players_[i].load(layers[i].data, layers[i].length);
        onload_gains_[i] = 0.0f;
        onload_target_gains_[i] = 0.0f;
    }
}

void EngineVoice::set_offload_layers(const LayerConfig* layers, uint8_t num_layers)
{
    num_offload_ = std::min(static_cast<uint8_t>(kMaxLayers), num_layers);
    for (uint8_t i = 0; i < num_offload_; ++i) {
        offload_configs_[i] = layers[i];
        offload_players_[i].load(layers[i].data, layers[i].length);
        offload_gains_[i] = 0.0f;
        offload_target_gains_[i] = 0.0f;
    }
}

void EngineVoice::set_onload_layers_auto(const LayerConfig* layers, uint8_t num_layers)
{
    num_onload_ = std::min(static_cast<uint8_t>(kMaxLayers), num_layers);
    for (uint8_t i = 0; i < num_onload_; ++i) {
        onload_configs_[i] = layers[i];
    }
    auto_fill_envelopes(onload_configs_, num_onload_);
    for (uint8_t i = 0; i < num_onload_; ++i) {
        onload_players_[i].load(onload_configs_[i].data, onload_configs_[i].length);
        onload_gains_[i] = 0.0f;
        onload_target_gains_[i] = 0.0f;
    }
}

void EngineVoice::set_offload_layers_auto(const LayerConfig* layers, uint8_t num_layers)
{
    num_offload_ = std::min(static_cast<uint8_t>(kMaxLayers), num_layers);
    for (uint8_t i = 0; i < num_offload_; ++i) {
        offload_configs_[i] = layers[i];
    }
    auto_fill_envelopes(offload_configs_, num_offload_);
    for (uint8_t i = 0; i < num_offload_; ++i) {
        offload_players_[i].load(offload_configs_[i].data, offload_configs_[i].length);
        offload_gains_[i] = 0.0f;
        offload_target_gains_[i] = 0.0f;
    }
}

void EngineVoice::set_engine_config(uint8_t cylinders, uint32_t sample_rate)
{
    cylinders_ = cylinders;
    sample_rate_ = sample_rate;

    // Initialize multi-band EQ
    float sr = static_cast<float>(sample_rate);
    eq_low_shelf_.set_peaking_eq(sr, 120.0f, 0.7f, 2.0f);
    eq_high_shelf_.set_peaking_eq(sr, 7000.0f, 0.5f, -1.5f);
}

void EngineVoice::process(const Params& params, sample_t* output, size_t frames)
{
    frames = std::min(frames, static_cast<size_t>(kDefaultBufferFrames));

    float dt = static_cast<float>(frames) / static_cast<float>(sample_rate_);
    float alpha = 1.0f - std::exp(-dt / gain_smooth_tc_);

    // --- Compute target gains (trapezoidal + equal-power normalize) ---
    {
        float sum_sq = 0.0f;
        for (uint8_t i = 0; i < num_onload_; ++i) {
            float g = compute_trapezoid_gain(params.rpm, onload_configs_[i]);
            onload_target_gains_[i] = g;
            sum_sq += g * g;
        }
        // Equal-power normalization: ensures constant perceived loudness
        if (sum_sq > 0.01f) {
            float scale = 1.0f / std::sqrt(sum_sq);
            for (uint8_t i = 0; i < num_onload_; ++i) {
                onload_target_gains_[i] *= scale;
            }
        }
    }
    {
        float sum_sq = 0.0f;
        for (uint8_t i = 0; i < num_offload_; ++i) {
            float g = compute_trapezoid_gain(params.rpm, offload_configs_[i]);
            offload_target_gains_[i] = g;
            sum_sq += g * g;
        }
        if (sum_sq > 0.01f) {
            float scale = 1.0f / std::sqrt(sum_sq);
            for (uint8_t i = 0; i < num_offload_; ++i) {
                offload_target_gains_[i] *= scale;
            }
        }
    }

    // --- Smooth gains ---
    smooth_gains(onload_target_gains_, onload_gains_, num_onload_, alpha);
    smooth_gains(offload_target_gains_, offload_gains_, num_offload_, alpha);

    // --- Mix all onload layers (pitch-shifted per layer) ---
    std::memset(buf_onload_f_, 0, frames * sizeof(float));
    for (uint8_t i = 0; i < num_onload_; ++i) {
        float rate = compute_layer_pitch(params.rpm, onload_configs_[i]);

        if (onload_gains_[i] < 0.001f) {
            onload_players_[i].process(buf_layer_, frames, rate);
            continue;
        }
        onload_players_[i].process(buf_layer_, frames, rate);
        float g = onload_gains_[i];
        for (size_t s = 0; s < frames; ++s) {
            buf_onload_f_[s] += static_cast<float>(buf_layer_[s]) * g;
        }
    }

    // --- Mix all offload layers (pitch-shifted per layer) ---
    std::memset(buf_offload_f_, 0, frames * sizeof(float));
    for (uint8_t i = 0; i < num_offload_; ++i) {
        float rate = compute_layer_pitch(params.rpm, offload_configs_[i]);

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
    float on_gain = std::sin(throttle * 1.5707963f);
    float off_gain = std::cos(throttle * 1.5707963f);

    for (size_t s = 0; s < frames; ++s) {
        float sample = buf_onload_f_[s] * on_gain + buf_offload_f_[s] * off_gain;
        sample = std::fmax(-32768.0f, std::fmin(32767.0f, sample));
        output[s] = static_cast<sample_t>(sample);
    }

    // --- Multi-band EQ ---
    float fundamental = params.rpm * static_cast<float>(cylinders_) / 120.0f;
    if (fundamental > 30.0f && fundamental < static_cast<float>(sample_rate_) / 2.0f) {
        eq_fundamental_.set_peaking_eq(static_cast<float>(sample_rate_),
            fundamental, 2.5f, 2.5f);
        eq_fundamental_.process(output, frames);
    }
    eq_low_shelf_.process(output, frames);
    eq_high_shelf_.process(output, frames);
}

void EngineVoice::reset()
{
    for (uint8_t i = 0; i < kMaxLayers; ++i) {
        onload_players_[i].reset();
        offload_players_[i].reset();
        onload_gains_[i] = 0.0f;
        offload_gains_[i] = 0.0f;
    }
    eq_fundamental_.reset();
    eq_low_shelf_.reset();
    eq_high_shelf_.reset();
}

} // namespace exhaust
