#include "core/transmission.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace exhaust {

Transmission::Transmission(const Config& config)
    : config_(config)
    , rpm_(config.rpm_idle)
    , gear_(1)
{
}

float Transmission::torque_at_rpm(float rpm) const
{
    // Simplified torque curve: parabolic shape peaking at peak_torque_rpm
    // T(rpm) = peak_torque * [1 - ((rpm - peak_rpm) / width)^2]
    // Width chosen so torque is ~60% at idle and ~70% at redline
    float peak_rpm = config_.peak_torque_rpm;
    float width = (config_.rpm_redline - config_.rpm_idle) * 0.7f;
    float x = (rpm - peak_rpm) / width;
    float t = config_.peak_torque * (1.0f - x * x);

    // Clamp: torque can't go negative from the curve
    if (t < 0.0f)
        t = 0.0f;

    // Past redline: torque drops rapidly (simulates valve float / fuel cut zone)
    if (rpm > config_.rpm_redline) {
        float over = (rpm - config_.rpm_redline) / 500.0f;
        t *= std::fmax(0.0f, 1.0f - over);
    }

    return t;
}

void Transmission::update(float throttle, float dt)
{
    // Clamp dt to avoid instability
    if (dt > 0.05f)
        dt = 0.05f;
    if (dt < 0.0001f)
        return;

    // --- Throttle smoothing (intake manifold lag) ---
    float smooth_rate = (throttle > smoothed_throttle_)
        ? config_.throttle_smooth_up
        : config_.throttle_smooth_down;
    float throttle_delta = (throttle - smoothed_throttle_) * smooth_rate * dt;
    smoothed_throttle_ += throttle_delta;
    smoothed_throttle_ = std::fmax(0.0f, std::fmin(1.0f, smoothed_throttle_));

    // --- Rev limiter (fuel cut) ---
    float effective_throttle = smoothed_throttle_;
    if (rev_limiter_active_) {
        // Cut fuel completely
        effective_throttle = 0.0f;
        // Release when RPM drops below threshold
        if (rpm_ < config_.rpm_redline - config_.rev_limiter_rpm_drop) {
            rev_limiter_active_ = false;
        }
    } else if (rpm_ >= config_.rpm_redline) {
        // Activate limiter
        rev_limiter_active_ = true;
        effective_throttle = 0.0f;
    }

    // --- During gear shift: cut torque ---
    if (shifting_) {
        shift_timer_ -= dt;
        if (shift_timer_ <= 0.0f) {
            shifting_ = false;
        }
        effective_throttle = 0.0f; // No torque during shift
    }

    // --- Idle controller (PID-like) ---
    float idle_throttle = 0.0f;
    if (rpm_ < config_.rpm_idle * 1.1f && effective_throttle < 0.01f) {
        // Simple proportional idle control
        idle_throttle = (config_.rpm_idle - rpm_) / config_.rpm_idle * 0.5f;
        idle_throttle = std::fmax(0.0f, std::fmin(0.15f, idle_throttle));
    }
    effective_throttle = std::fmax(effective_throttle, idle_throttle);

    // --- Torque calculation ---
    float combustion_torque = torque_at_rpm(rpm_) * effective_throttle;

    // --- Friction model ---
    float omega = rpm_ * static_cast<float>(M_PI) / 30.0f; // RPM to rad/s
    float friction_torque = config_.friction
        + config_.dynamic_friction * std::fabs(omega);

    // Engine braking (when throttle is closed)
    float engine_brake = 0.0f;
    if (effective_throttle < 0.05f && rpm_ > config_.rpm_idle * 1.2f) {
        engine_brake = config_.engine_brake * (1.0f - effective_throttle / 0.05f);
    }

    // --- Net torque and angular acceleration ---
    float net_torque = combustion_torque - friction_torque - engine_brake;

    // Sign of friction opposes motion
    if (omega > 0.0f && net_torque < -friction_torque) {
        net_torque = -friction_torque;
    }

    // Angular acceleration: α = τ / I
    float alpha = net_torque / config_.inertia;

    // Integrate: ω_new = ω + α·dt
    omega += alpha * dt;

    // Convert back to RPM
    rpm_ = omega * 30.0f / static_cast<float>(M_PI);

    // Clamp to minimum (engine doesn't stall in this sim)
    if (rpm_ < config_.rpm_idle * 0.8f) {
        rpm_ = config_.rpm_idle * 0.8f;
    }

    // --- Compute load ---
    load_ = (config_.peak_torque > 0.0f)
        ? std::fmax(0.0f, std::fmin(1.0f, combustion_torque / config_.peak_torque))
        : 0.0f;

    // --- Afterfire detection ---
    // Triggered by rapid load drop at high RPM (throttle lift)
    float load_rate = (load_ - prev_load_) / dt;
    prev_load_ = load_;

    if (load_rate < -2.0f && rpm_ > config_.rpm_idle * 2.5f) {
        // Instant afterfire: intensity proportional to load drop rate and RPM
        afterfire_ = std::fmin(1.0f, std::fabs(load_rate) * 0.2f * (rpm_ / config_.rpm_redline));
        afterfire_timer_ = 1.0f; // Sustain for up to 1 second
    } else if (afterfire_timer_ > 0.0f && load_ < 0.05f) {
        // Sustained overrun crackle (decaying)
        afterfire_timer_ -= dt;
        afterfire_ = afterfire_timer_ * 0.3f * (rpm_ / config_.rpm_redline);
    } else {
        afterfire_ = 0.0f;
        afterfire_timer_ = 0.0f;
    }
    afterfire_ = std::fmax(0.0f, std::fmin(1.0f, afterfire_));

    // --- Auto shift logic ---
    float rpm_fraction = (rpm_ - config_.rpm_idle) / (config_.rpm_redline - config_.rpm_idle);
    if (rpm_fraction >= config_.rpm_upshift && gear_ < config_.num_gears) {
        shift_up();
    } else if (rpm_fraction <= config_.rpm_downshift && gear_ > 1) {
        shift_down();
    }
}

void Transmission::shift_up()
{
    if (gear_ < config_.num_gears && !shifting_) {
        float old_ratio = config_.gear_ratios[gear_ - 1];
        gear_++;
        float new_ratio = config_.gear_ratios[gear_ - 1];
        shifting_ = true;
        shift_timer_ = 0.06f; // 60ms DCT shift
        rpm_ *= (new_ratio / old_ratio);
        rpm_ = std::fmax(config_.rpm_idle, rpm_);
    }
}

void Transmission::shift_down()
{
    if (gear_ > 1 && !shifting_) {
        float old_ratio = config_.gear_ratios[gear_ - 1];
        gear_--;
        float new_ratio = config_.gear_ratios[gear_ - 1];
        shifting_ = true;
        shift_timer_ = 0.08f; // 80ms DCT downshift (blip)
        rpm_ *= (new_ratio / old_ratio);
        rpm_ = std::fmin(config_.rpm_redline * 1.05f, rpm_);
    }
}

void Transmission::reset()
{
    rpm_ = config_.rpm_idle;
    gear_ = 1;
    load_ = 0.0f;
    shifting_ = false;
    shift_timer_ = 0.0f;
    smoothed_throttle_ = 0.0f;
    rev_limiter_active_ = false;
    afterfire_ = 0.0f;
    prev_load_ = 0.0f;
    afterfire_timer_ = 0.0f;
}

} // namespace exhaust
