#include "core/engine_effects.h"

#include <cmath>
#include <cstring>

namespace exhaust {

// ============================================================
// CombustionPulse
// ============================================================

void CombustionPulse::load_pulse(const sample_t* data, size_t length)
{
    pulse_data_ = data;
    pulse_length_ = length;
}

void CombustionPulse::set_config(const Config& config)
{
    config_ = config;
}

void CombustionPulse::process(float* output, size_t frames, float rpm, uint32_t sample_rate)
{
    if (!pulse_data_ || pulse_length_ == 0 || rpm < 100.0f)
        return;

    // Firing interval in samples:
    // At RPM, each cylinder fires every (120 / RPM / cylinders) seconds
    // = (120 * sample_rate) / (RPM * cylinders) samples between firings
    // But for a 4-stroke engine, each cylinder fires once per 2 revolutions,
    // so total firings per second = RPM * cylinders / 120
    float firings_per_second = rpm * config_.cylinders / 120.0f;
    float interval_samples = static_cast<float>(sample_rate) / firings_per_second;

    // Volume fade based on RPM (pulses less audible at high RPM)
    float rpm_fade = 1.0f;
    if (rpm > config_.rpm_fade_start) {
        rpm_fade = 1.0f - (rpm - config_.rpm_fade_start) / (config_.rpm_fade_end - config_.rpm_fade_start);
        rpm_fade = std::fmax(0.0f, std::fmin(1.0f, rpm_fade));
    }

    float base_volume = config_.volume * rpm_fade;
    if (base_volume < 0.001f)
        return;

    for (size_t i = 0; i < frames; ++i) {
        // If a pulse is currently playing, output it
        if (pulse_active_ && pulse_pos_ < pulse_length_) {
            float cyl_vol = config_.cyl_volumes[current_cyl_ % 8];
            output[i] += static_cast<float>(pulse_data_[pulse_pos_]) * base_volume * cyl_vol;
            pulse_pos_++;
            if (pulse_pos_ >= pulse_length_) {
                pulse_active_ = false;
            }
        }

        // Advance firing accumulator
        phase_acc_ += 1.0;
        if (phase_acc_ >= interval_samples) {
            phase_acc_ -= interval_samples;
            // Trigger next cylinder
            current_cyl_ = (current_cyl_ + 1) % config_.cylinders;
            pulse_active_ = true;
            pulse_pos_ = 0;
        }
    }
}

void CombustionPulse::reset()
{
    phase_acc_ = 0.0f;
    current_cyl_ = 0;
    pulse_pos_ = 0;
    pulse_active_ = false;
}

// ============================================================
// IdleFluctuation
// ============================================================

float IdleFluctuation::update(float rpm, float dt)
{
    if (rpm > config_.rpm_threshold)
        return 0.0f;

    // Advance phase
    phase_ += config_.rate * dt;
    if (phase_ > 1.0f)
        phase_ -= 1.0f;

    // Combine slow sine with filtered noise for natural feel
    float sine_component = std::sin(phase_ * 6.2831853f);

    // Simple filtered noise (random walk with decay)
    // Use a cheap PRNG-like approach based on phase
    float noise_input = std::sin(phase_ * 17.3f) * std::cos(phase_ * 31.7f);
    noise_state_ = noise_state_ * 0.95f + noise_input * 0.05f;

    float fluctuation = (sine_component * 0.6f + noise_state_ * 0.4f) * config_.amplitude;

    // Fade out as RPM rises above idle
    float fade = 1.0f - (rpm - 800.0f) / (config_.rpm_threshold - 800.0f);
    fade = std::fmax(0.0f, std::fmin(1.0f, fade));

    return fluctuation * fade;
}

void IdleFluctuation::reset()
{
    phase_ = 0.0f;
    noise_state_ = 0.0f;
}

// ============================================================
// ThrottleTransient
// ============================================================

float ThrottleTransient::update(float throttle, float dt)
{
    // Detect throttle onset (rising edge)
    float delta = throttle - prev_throttle_;
    prev_throttle_ = throttle;

    if (delta > 0.1f && !attacking_) {
        // Throttle just applied — trigger attack
        attacking_ = true;
        attack_timer_ = 0.0f;
        envelope_ = config_.attack_gain;
    }

    if (attacking_) {
        attack_timer_ += dt;
        if (attack_timer_ < config_.attack_time) {
            // Hold at peak
            envelope_ = config_.attack_gain;
        } else {
            // Decay back to 1.0
            float decay_progress = (attack_timer_ - config_.attack_time) / config_.decay_time;
            if (decay_progress >= 1.0f) {
                envelope_ = 1.0f;
                attacking_ = false;
            } else {
                envelope_ = config_.attack_gain + (1.0f - config_.attack_gain) * decay_progress;
            }
        }
    } else {
        // Smoothly return to 1.0
        envelope_ += (1.0f - envelope_) * dt * 10.0f;
    }

    return envelope_;
}

void ThrottleTransient::reset()
{
    prev_throttle_ = 0.0f;
    envelope_ = 1.0f;
    attacking_ = false;
    attack_timer_ = 0.0f;
}

// ============================================================
// RevLimiter
// ============================================================

float RevLimiter::next_random()
{
    // xorshift32
    rng_state_ ^= rng_state_ << 13;
    rng_state_ ^= rng_state_ >> 17;
    rng_state_ ^= rng_state_ << 5;
    return static_cast<float>(rng_state_ & 0xFFFF) / 65536.0f;
}

float RevLimiter::update(float rpm)
{
    if (rpm >= config_.rpm_limit) {
        active_ = true;
    } else if (rpm < config_.rpm_limit - config_.rpm_hysteresis) {
        active_ = false;
    }

    if (active_) {
        // Randomly cut fuel — not full silence, just reduced
        if (next_random() < config_.cut_probability) {
            return 0.3f; // Partial cut (not full silence to avoid pop)
        }
        return 0.6f; // Reduced power between cuts
    }

    return 1.0f; // Normal
}

void RevLimiter::reset()
{
    active_ = false;
}

} // namespace exhaust
