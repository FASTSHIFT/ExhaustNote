#pragma once

#include "core/car_config.h"
#include "core/engine_effects.h"
#include "core/engine_voice.h"
#include "core/mixer.h"
#include "core/transmission.h"
#include "core/types.h"

#include <SDL2/SDL.h>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace exhaust {

/// Audio engine state — holds all sound processing objects.
struct AudioState {
    std::mutex mutex;
    EngineVoice engine;
    Mixer mixer;
    CombustionPulse combustion;
    ThrottleTransient transient;
    IdleFluctuation idle_fluct;
    EngineVoice::Params params;
    float throttle = 0.0f;
    float dt = 0.016f;
    bool engine_running = false;

    // Backfire one-shot
    std::vector<sample_t> backfire_sample;
    size_t backfire_pos = 0;
    bool backfire_playing = false;
    float afterfire_level = 0.0f;
};

/// SDL audio callback — fills the output buffer from AudioState.
void audio_callback(void* userdata, Uint8* stream, int len);

/// Load a car from JSON config, set up engine voice + transmission.
/// @param json_path     Path to car.json
/// @param audio_state   Audio state to configure
/// @param transmission  Transmission to configure
/// @param pulse_sample  Combustion pulse sample data
/// @param pulse_len     Pulse sample length
/// @return true on success
bool load_car(const std::string& json_path,
    AudioState& audio_state, Transmission& transmission,
    const sample_t* pulse_sample, size_t pulse_len);

/// Get the currently loaded car config (read-only).
const CarConfig& current_car_config();

/// Generate the synthetic combustion pulse sample.
/// @param buffer  Output buffer (at least 512 samples)
/// @param length  Buffer length
void generate_combustion_pulse(sample_t* buffer, size_t length);

/// Load backfire sample from file.
/// @param path         Path to WAV file
/// @param audio_state  Audio state to store sample in
/// @return true on success
bool load_backfire_sample(const std::string& path, AudioState& audio_state);

} // namespace exhaust
