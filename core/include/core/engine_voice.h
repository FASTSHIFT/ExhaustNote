#pragma once

#include "core/crossfade.h"
#include "core/dsp.h"
#include "core/mixer.h"
#include "core/sample_player.h"
#include "core/types.h"

namespace exhaust {

/// Multi-layer crossfade engine sound synthesizer.
///
/// All layers play simultaneously with RPM-driven gain envelopes.
/// This eliminates click artifacts from layer switching.
/// Uses equal-power (Gaussian) gain curves for smooth transitions.
class EngineVoice {
public:
    struct Params {
        float rpm = 800.0f; ///< Current engine RPM
        float throttle = 0.0f; ///< Throttle position [0, 1]
        float load = 0.0f; ///< Engine load [0, 1]
    };

    struct LayerConfig {
        float rpm; ///< RPM this layer was recorded at
        const sample_t* data; ///< PCM sample data
        size_t length; ///< Number of samples
    };

    EngineVoice() = default;

    /// Configure the onload (throttle open) layers.
    void set_onload_layers(const LayerConfig* layers, uint8_t num_layers);

    /// Configure the offload (throttle closed) layers.
    void set_offload_layers(const LayerConfig* layers, uint8_t num_layers);

    /// Set engine parameters (cylinders, for EQ tracking).
    void set_engine_config(uint8_t cylinders, uint32_t sample_rate = kDefaultSampleRate);

    /// Set gain smoothing time constant (seconds). Default 0.05s.
    void set_gain_smoothing(float time_constant) { gain_smooth_tc_ = time_constant; }

    /// Process one block of audio.
    void process(const Params& params, sample_t* output, size_t frames);

    /// Reset all internal state.
    void reset();

private:
    /// Compute Gaussian gain for a layer given current RPM.
    void compute_layer_gains(float rpm, const float* layer_rpms, uint8_t num_layers,
        float* gains_out);

    /// Smooth gains with exponential filter to avoid clicks.
    void smooth_gains(float* target, float* current, uint8_t count, float alpha);

    // Onload layers
    SamplePlayer onload_players_[kMaxLayers];
    float onload_rpms_[kMaxLayers] = {};
    float onload_gains_[kMaxLayers] = {}; ///< Current smoothed gains
    float onload_target_gains_[kMaxLayers] = {}; ///< Target gains
    uint8_t num_onload_ = 0;

    // Offload layers
    SamplePlayer offload_players_[kMaxLayers];
    float offload_rpms_[kMaxLayers] = {};
    float offload_gains_[kMaxLayers] = {};
    float offload_target_gains_[kMaxLayers] = {};
    uint8_t num_offload_ = 0;

    // DSP
    BiquadFilter eq_;
    uint8_t cylinders_ = 4;
    uint32_t sample_rate_ = kDefaultSampleRate;
    float gain_smooth_tc_ = 0.05f; ///< Gain smoothing time constant (seconds)

    // Temp buffers
    sample_t buf_layer_[kDefaultBufferFrames] = {};
    float buf_onload_f_[kDefaultBufferFrames] = {};
    float buf_offload_f_[kDefaultBufferFrames] = {};
};

} // namespace exhaust
