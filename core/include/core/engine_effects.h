#pragma once

#include "core/types.h"
#include <cstdint>

namespace exhaust {

/// Combustion pulse overlay — simulates individual cylinder firing events.
///
/// At low RPM, you can hear each cylinder fire individually ("thud-thud-thud").
/// As RPM rises, pulses blur together into the engine roar.
/// KEY: The pulse sample plays at FIXED pitch (no pitch shift).
/// Only the repetition interval changes with RPM.
class CombustionPulse {
public:
    struct Config {
        uint8_t cylinders = 8; ///< Number of cylinders
        float volume = 0.6f; ///< Pulse volume (0-1)
        float rpm_fade_start = 4000.0f; ///< Start fading out pulses above this RPM
        float rpm_fade_end = 7000.0f; ///< Fully faded out above this RPM
        /// Per-cylinder volume variation (models exhaust manifold pairing).
        /// For V8: cylinders sharing a manifold are louder.
        float cyl_volumes[8] = { 1.0f, 0.8f, 1.0f, 0.9f, 1.0f, 0.8f, 1.0f, 0.9f };
    };

    CombustionPulse() = default;

    /// Load the combustion pulse sample (short one-shot, ~5-20ms).
    /// @param data    PCM sample data (fixed pitch, not shifted)
    /// @param length  Number of samples
    void load_pulse(const sample_t* data, size_t length);

    /// Set configuration.
    void set_config(const Config& config);

    /// Process: add combustion pulses to the output buffer.
    /// @param output      Buffer to ADD pulses into (not overwrite)
    /// @param frames      Number of frames
    /// @param rpm         Current engine RPM
    /// @param sample_rate Audio sample rate
    void process(float* output, size_t frames, float rpm, uint32_t sample_rate);

    /// Reset state.
    void reset();

private:
    Config config_;
    const sample_t* pulse_data_ = nullptr;
    size_t pulse_length_ = 0;

    // State
    double phase_acc_ = 0.0; ///< Accumulator for firing interval
    uint8_t current_cyl_ = 0; ///< Current cylinder index
    size_t pulse_pos_ = 0; ///< Current position in pulse playback
    bool pulse_active_ = false; ///< Is a pulse currently playing?
};

/// Idle RPM fluctuation — adds natural "breathing" to idle.
///
/// Real engines hunt ±30-50 RPM at idle due to combustion variation,
/// accessory load changes, and ECU feedback loop oscillation.
class IdleFluctuation {
public:
    struct Config {
        float amplitude = 30.0f; ///< RPM fluctuation amplitude (±)
        float rate = 1.2f; ///< Fluctuation rate in Hz
        float rpm_threshold = 1200.0f; ///< Only active below this RPM
    };

    IdleFluctuation() = default;

    void set_config(const Config& config) { config_ = config; }

    /// Get RPM offset to add to current RPM.
    /// @param rpm  Current base RPM
    /// @param dt   Time step in seconds
    /// @return RPM offset (positive or negative)
    float update(float rpm, float dt);

    void reset();

private:
    Config config_;
    float phase_ = 0.0f;
    float noise_state_ = 0.0f;
};

/// Throttle transient — models the "attack" when throttle is applied.
///
/// When throttle goes from 0 to >0, there's a brief volume/gain overshoot
/// that gives the sound a punchy "blip" character.
class ThrottleTransient {
public:
    struct Config {
        float attack_gain = 1.3f; ///< Peak gain multiplier during attack
        float attack_time = 0.08f; ///< Attack duration in seconds
        float decay_time = 0.2f; ///< Decay back to 1.0 duration
    };

    ThrottleTransient() = default;

    void set_config(const Config& config) { config_ = config; }

    /// Update and return current gain multiplier.
    /// @param throttle  Current throttle position [0, 1]
    /// @param dt        Time step in seconds
    /// @return Gain multiplier (1.0 = no effect, >1.0 = boosted)
    float update(float throttle, float dt);

    void reset();

private:
    Config config_;
    float prev_throttle_ = 0.0f;
    float envelope_ = 1.0f; ///< Current gain envelope
    bool attacking_ = false;
    float attack_timer_ = 0.0f;
};

/// Rev limiter — simulates ECU fuel cut at redline.
///
/// When RPM hits the limiter, randomly cuts engine output for 1-2 frames,
/// creating the characteristic "bap-bap-bap" bouncing off the limiter.
class RevLimiter {
public:
    struct Config {
        float rpm_limit = 9000.0f; ///< RPM at which limiter activates
        float rpm_hysteresis = 100.0f; ///< RPM below limit to deactivate
        float cut_probability = 0.7f; ///< Probability of cutting per frame
    };

    RevLimiter() = default;

    void set_config(const Config& config) { config_ = config; }

    /// Update and return whether engine should be cut this frame.
    /// @param rpm  Current RPM
    /// @return Gain multiplier (0.0 = cut, 1.0 = normal)
    float update(float rpm);

    void reset();

private:
    Config config_;
    bool active_ = false;
    uint32_t rng_state_ = 12345; ///< Simple PRNG state
    float next_random(); ///< Returns [0, 1)
};

} // namespace exhaust
