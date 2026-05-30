#pragma once

#include "core/types.h"

namespace exhaust {

/// Physics-based engine + transmission simulation.
///
/// Models: torque curve, rotational inertia, friction, rev limiter (fuel cut),
/// throttle smoothing, and afterfire (overrun pops).
/// Based on publicly documented game engine physics models.
class Transmission {
public:
    struct Config {
        float gear_ratios[8] = { 3.82f, 2.36f, 1.68f, 1.31f, 1.00f, 0.79f };
        uint8_t num_gears = 6;
        float final_drive = 3.73f;
        float rpm_upshift = 0.85f; ///< Upshift at this fraction of redline
        float rpm_downshift = 0.35f; ///< Downshift at this fraction of redline
        float rpm_idle = 800.0f;
        float rpm_redline = 8000.0f;

        // Physics parameters
        float inertia = 0.12f; ///< Crankshaft + flywheel inertia (kg·m²)
        float peak_torque = 530.0f; ///< Peak torque (Nm) at peak_torque_rpm
        float peak_torque_rpm = 6000.0f; ///< RPM at peak torque
        float friction = 15.0f; ///< Static friction torque (Nm)
        float dynamic_friction = 0.005f; ///< Speed-proportional friction (Nm·s/rad)
        float engine_brake = 30.0f; ///< Engine braking torque on overrun (Nm)

        // Rev limiter (fuel cut)
        float rev_limiter_rpm_drop = 200.0f; ///< RPM drop before re-engaging

        // Throttle smoothing (intake manifold lag)
        float throttle_smooth_up = 25.0f; ///< Throttle rise rate (1/s)
        float throttle_smooth_down = 12.0f; ///< Throttle fall rate (1/s)
    };

    Transmission() = default;
    explicit Transmission(const Config& config);

    /// Update engine physics.
    /// @param throttle  Throttle position [0, 1] (raw input)
    /// @param dt        Time step in seconds
    void update(float throttle, float dt);

    /// Get current RPM.
    float rpm() const { return rpm_; }

    /// Get current gear (1-based, 0 = neutral).
    uint8_t gear() const { return gear_; }

    /// Get current engine load [0, 1] (instantaneous torque / peak torque).
    float load() const { return load_; }

    /// Is the rev limiter currently active (fuel cut)?
    bool rev_limiter_active() const { return rev_limiter_active_; }

    /// Get afterfire intensity [0, 1] (for triggering pop sounds).
    float afterfire() const { return afterfire_; }

    /// Manual shift up.
    void shift_up();

    /// Manual shift down.
    void shift_down();

    /// Reset to idle in first gear.
    void reset();

private:
    /// Evaluate torque curve at given RPM.
    float torque_at_rpm(float rpm) const;

    Config config_;
    float rpm_ = 800.0f;
    float load_ = 0.0f;
    uint8_t gear_ = 1;
    float shift_timer_ = 0.0f;
    bool shifting_ = false;

    // Smoothed throttle (simulates intake manifold lag)
    float smoothed_throttle_ = 0.0f;

    // Rev limiter state
    bool rev_limiter_active_ = false;

    // Afterfire state
    float afterfire_ = 0.0f;
    float prev_load_ = 0.0f;
    float afterfire_timer_ = 0.0f;
};

} // namespace exhaust
