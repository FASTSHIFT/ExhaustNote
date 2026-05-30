#pragma once

#include "core/crossfade.h"
#include "core/dsp.h"
#include "core/mixer.h"
#include "core/sample_player.h"
#include "core/types.h"

namespace exhaust {

/// Multi-layer crossfade engine sound synthesizer.
///
/// This is the main audio engine that blends multiple RPM-sampled layers
/// with on/off throttle states to produce realistic engine sound.
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
    /// @param layers      Array of layer configurations
    /// @param num_layers  Number of layers
    void set_onload_layers(const LayerConfig* layers, uint8_t num_layers);

    /// Configure the offload (throttle closed) layers.
    /// @param layers      Array of layer configurations
    /// @param num_layers  Number of layers
    void set_offload_layers(const LayerConfig* layers, uint8_t num_layers);

    /// Set engine parameters (cylinders, for EQ tracking).
    /// @param cylinders   Number of cylinders
    /// @param sample_rate Audio sample rate
    void set_engine_config(uint8_t cylinders, uint32_t sample_rate = kDefaultSampleRate);

    /// Process one block of audio.
    /// @param params  Current engine parameters
    /// @param output  Output buffer to fill
    /// @param frames  Number of frames to generate
    void process(const Params& params, sample_t* output, size_t frames);

    /// Reset all internal state.
    void reset();

private:
    // Onload layers
    SamplePlayer onload_players_[kMaxLayers];
    float onload_rpms_[kMaxLayers] = {};
    uint8_t num_onload_ = 0;

    // Offload layers
    SamplePlayer offload_players_[kMaxLayers];
    float offload_rpms_[kMaxLayers] = {};
    uint8_t num_offload_ = 0;

    // DSP
    BiquadFilter eq_;
    uint8_t cylinders_ = 4;
    uint32_t sample_rate_ = kDefaultSampleRate;

    // Temp buffers
    sample_t buf_lo_[kDefaultBufferFrames] = {};
    sample_t buf_hi_[kDefaultBufferFrames] = {};
    sample_t buf_onload_[kDefaultBufferFrames] = {};
    sample_t buf_offload_[kDefaultBufferFrames] = {};
};

} // namespace exhaust
