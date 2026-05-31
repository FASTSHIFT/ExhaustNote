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
        float rpm_upshift = 0.97f; ///< Upshift at this fraction of redline (near redline)
        float rpm_downshift = 0.35f; ///< Downshift at this fraction of redline
        bool auto_shift = false; ///< Enable automatic gear shifting
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

    /// Get approximate vehicle speed in km/h.
    /// Assumes wheel radius of 0.33m.
    float speed_kmh() const
    {
        if (gear_ == 0 || config_.num_gears == 0)
            return 0.0f;
        float overall = config_.gear_ratios[gear_ - 1] * config_.final_drive;
        if (overall < 0.01f)
            return 0.0f;
        // speed = RPM / overall_ratio * wheel_circumference / 60
        // wheel_circ = 2 * pi * 0.33 = 2.073m
        return (rpm_ / overall) * 2.073f * 60.0f / 1000.0f;
    }

    /// Set external load torque (Nm). Simulates drivetrain resistance.
    void set_external_load(float torque_nm) { external_load_ = torque_nm; }
    float external_load() const { return external_load_; }

    /// Set engine braking torque (Nm). Higher = faster deceleration on overrun.
    void set_engine_brake(float nm) { config_.engine_brake = nm; }
    float engine_brake() const { return config_.engine_brake; }

    /// Set road load coefficient. Higher = more speed-dependent drag.
    void set_road_load_coeff(float c) { road_load_coeff_ = c; }
    float road_load_coeff() const { return road_load_coeff_; }

    /// Set brake torque (Nm). Applied as pure resistance without affecting inertia.
    void set_brake_torque(float nm) { brake_torque_ = nm; }
    float brake_torque() const { return brake_torque_; }

    /// Mutable access to config (for runtime toggles like auto_shift).
    Config& config_mut() { return config_; }

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

    // External load (drivetrain resistance)
    float external_load_ = 0.0f;
    float road_load_coeff_ = 0.3f; ///< Road load multiplier (0.1=light, 0.3=normal, 0.6=heavy)
    float brake_torque_ = 0.0f; ///< Brake pedal torque (pure resistance, no inertia effect)

    // Rev limiter state
    bool rev_limiter_active_ = false;

    // Afterfire state
    float afterfire_ = 0.0f;
    float prev_load_ = 0.0f;
    float afterfire_timer_ = 0.0f;
};

} // namespace exhaust
