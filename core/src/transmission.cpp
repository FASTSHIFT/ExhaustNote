#include "core/transmission.h"

#include <algorithm>
#include <cmath>

namespace exhaust {

Transmission::Transmission(const Config& config)
    : config_(config)
    , rpm_(config.rpm_idle)
    , gear_(1)
{
}

void Transmission::update(float throttle, float dt)
{
    if (shifting_) {
        shift_timer_ -= dt;
        if (shift_timer_ <= 0.0f) {
            shifting_ = false;
        }
        // During shift, RPM decays slightly
        rpm_ += (config_.rpm_idle - rpm_) * dt * 3.0f;
        load_ = 0.0f;
        return;
    }

    // Simple RPM model: throttle drives RPM up, friction drives it down
    float target_rpm = config_.rpm_idle + throttle * (config_.rpm_redline - config_.rpm_idle);
    float rpm_rate = (target_rpm > rpm_) ? 3000.0f : 2000.0f; // RPM/s acceleration
    float delta = (target_rpm - rpm_);
    float step = rpm_rate * dt;

    if (std::fabs(delta) < step) {
        rpm_ = target_rpm;
    } else {
        rpm_ += (delta > 0 ? step : -step);
    }

    rpm_ = std::fmax(config_.rpm_idle, std::fmin(config_.rpm_redline, rpm_));

    // Compute load
    load_ = std::fmax(0.0f, std::fmin(1.0f, throttle));

    // Auto shift logic
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
        // RPM after shift = RPM_before × (new_gear_ratio / old_gear_ratio)
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
        rpm_ = std::fmin(config_.rpm_redline, rpm_);
    }
}

void Transmission::reset()
{
    rpm_ = config_.rpm_idle;
    gear_ = 1;
    load_ = 0.0f;
    shifting_ = false;
    shift_timer_ = 0.0f;
}

} // namespace exhaust
