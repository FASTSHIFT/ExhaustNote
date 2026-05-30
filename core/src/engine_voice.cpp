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
    }
}

void EngineVoice::set_offload_layers(const LayerConfig* layers, uint8_t num_layers)
{
    num_offload_ = std::min(static_cast<uint8_t>(kMaxLayers), num_layers);
    for (uint8_t i = 0; i < num_offload_; ++i) {
        offload_rpms_[i] = layers[i].rpm;
        offload_players_[i].load(layers[i].data, layers[i].length);
    }
}

void EngineVoice::set_engine_config(uint8_t cylinders, uint32_t sample_rate)
{
    cylinders_ = cylinders;
    sample_rate_ = sample_rate;
}

void EngineVoice::process(const Params& params, sample_t* output, size_t frames)
{
    frames = std::min(frames, static_cast<size_t>(kDefaultBufferFrames));

    // --- Onload layers ---
    if (num_onload_ > 0) {
        CrossfadeResult cf = compute_crossfade(params.rpm, onload_rpms_, num_onload_);
        onload_players_[cf.layer_lo].process(buf_lo_, frames);
        if (cf.layer_lo != cf.layer_hi) {
            onload_players_[cf.layer_hi].process(buf_hi_, frames);
            mix_layers(buf_lo_, buf_hi_, buf_onload_, frames, cf.mix);
        } else {
            std::memcpy(buf_onload_, buf_lo_, frames * sizeof(sample_t));
        }
    } else {
        std::memset(buf_onload_, 0, frames * sizeof(sample_t));
    }

    // --- Offload layers ---
    if (num_offload_ > 0) {
        CrossfadeResult cf = compute_crossfade(params.rpm, offload_rpms_, num_offload_);
        offload_players_[cf.layer_lo].process(buf_lo_, frames);
        if (cf.layer_lo != cf.layer_hi) {
            offload_players_[cf.layer_hi].process(buf_hi_, frames);
            mix_layers(buf_lo_, buf_hi_, buf_offload_, frames, cf.mix);
        } else {
            std::memcpy(buf_offload_, buf_lo_, frames * sizeof(sample_t));
        }
    } else {
        std::memset(buf_offload_, 0, frames * sizeof(sample_t));
    }

    // --- Blend onload/offload by throttle ---
    float throttle = std::fmax(0.0f, std::fmin(1.0f, params.throttle));
    mix_layers(buf_offload_, buf_onload_, output, frames, throttle);

    // --- Apply EQ tracking engine fundamental frequency ---
    float fundamental = params.rpm * static_cast<float>(cylinders_) / 120.0f;
    if (fundamental > 20.0f && fundamental < static_cast<float>(sample_rate_) / 2.0f) {
        eq_.set_peaking_eq(static_cast<float>(sample_rate_), fundamental, 2.0f, 3.0f);
        eq_.process(output, frames);
    }
}

void EngineVoice::reset()
{
    for (uint8_t i = 0; i < kMaxLayers; ++i) {
        onload_players_[i].reset();
        offload_players_[i].reset();
    }
    eq_.reset();
}

} // namespace exhaust
