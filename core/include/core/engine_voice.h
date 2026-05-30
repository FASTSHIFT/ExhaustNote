#pragma once

#include "core/crossfade.h"
#include "core/dsp.h"
#include "core/mixer.h"
#include "core/sample_player.h"
#include "core/types.h"

namespace exhaust {

/// Multi-layer crossfade engine sound synthesizer.
///
/// Features:
/// - All layers play simultaneously with trapezoidal volume envelopes
/// - Per-layer independent pitch range (min/max pitch)
/// - Equal-power on/off throttle blending
/// - Multi-band EQ with fundamental frequency tracking
/// - Exponential gain smoothing to prevent clicks
class EngineVoice {
public:
    struct Params {
        float rpm = 800.0f; ///< Current engine RPM
        float throttle = 0.0f; ///< Throttle position [0, 1]
        float load = 0.0f; ///< Engine load [0, 1]
    };

    /// Per-layer configuration with trapezoidal envelope and pitch range.
    struct LayerConfig {
        float rpm; ///< RPM this layer was recorded at
        const sample_t* data; ///< PCM sample data
        size_t length; ///< Number of samples

        // Trapezoidal volume envelope (RPM values)
        // Volume ramps from 0→1 over [vol_in_start, vol_in_end]
        // Holds at 1.0 over [vol_in_end, vol_out_start]
        // Ramps from 1→0 over [vol_out_start, vol_out_end]
        float vol_in_start = 0; ///< Volume fade-in start RPM
        float vol_in_end = 0; ///< Volume fade-in end RPM (full volume)
        float vol_out_start = 0; ///< Volume fade-out start RPM
        float vol_out_end = 0; ///< Volume fade-out end RPM (silent)

        // Per-layer pitch range
        float min_pitch = 0.5f; ///< Minimum playback rate
        float max_pitch = 2.0f; ///< Maximum playback rate
        float pitch_start_rpm = 0; ///< RPM at which pitch = min_pitch
        float pitch_end_rpm = 0; ///< RPM at which pitch = max_pitch
    };

    EngineVoice() = default;

    /// Configure onload layers with full trapezoidal envelope.
    void set_onload_layers(const LayerConfig* layers, uint8_t num_layers);

    /// Configure offload layers with full trapezoidal envelope.
    void set_offload_layers(const LayerConfig* layers, uint8_t num_layers);

    /// Auto-generate trapezoidal envelopes from simple RPM-only config.
    /// Convenience method: computes overlapping envelopes automatically.
    void set_onload_layers_auto(const LayerConfig* layers, uint8_t num_layers);
    void set_offload_layers_auto(const LayerConfig* layers, uint8_t num_layers);

    /// Set engine parameters (cylinders, for EQ tracking).
    void set_engine_config(uint8_t cylinders, uint32_t sample_rate = kDefaultSampleRate);

    /// Set gain smoothing time constant (seconds). Default 0.05s.
    void set_gain_smoothing(float time_constant) { gain_smooth_tc_ = time_constant; }

    /// Process one block of audio.
    void process(const Params& params, sample_t* output, size_t frames);

    /// Reset all internal state.
    void reset();

private:
    /// Compute trapezoidal gain for a single layer.
    static float compute_trapezoid_gain(float rpm, const LayerConfig& layer);

    /// Compute pitch rate for a single layer.
    static float compute_layer_pitch(float rpm, const LayerConfig& layer);

    /// Auto-fill envelope parameters for a layer set.
    static void auto_fill_envelopes(LayerConfig* layers, uint8_t num_layers);

    /// Smooth gains with exponential filter.
    void smooth_gains(float* target, float* current, uint8_t count, float alpha);

    // Onload layers
    SamplePlayer onload_players_[kMaxLayers];
    LayerConfig onload_configs_[kMaxLayers] = {};
    float onload_gains_[kMaxLayers] = {};
    float onload_target_gains_[kMaxLayers] = {};
    uint8_t num_onload_ = 0;

    // Offload layers
    SamplePlayer offload_players_[kMaxLayers];
    LayerConfig offload_configs_[kMaxLayers] = {};
    float offload_gains_[kMaxLayers] = {};
    float offload_target_gains_[kMaxLayers] = {};
    uint8_t num_offload_ = 0;

    // Multi-band EQ
    BiquadFilter eq_fundamental_; ///< Tracks engine firing frequency
    BiquadFilter eq_low_shelf_; ///< Low shelf (warmth)
    BiquadFilter eq_high_shelf_; ///< High shelf (presence)
    uint8_t cylinders_ = 4;
    uint32_t sample_rate_ = kDefaultSampleRate;
    float gain_smooth_tc_ = 0.05f;

    // Temp buffers
    sample_t buf_layer_[kDefaultBufferFrames] = {};
    float buf_onload_f_[kDefaultBufferFrames] = {};
    float buf_offload_f_[kDefaultBufferFrames] = {};
};

} // namespace exhaust
