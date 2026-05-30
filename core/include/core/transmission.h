#pragma once

#include "core/types.h"

namespace exhaust {

/// Simple automatic transmission simulation.
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
    };

    Transmission() = default;
    explicit Transmission(const Config& config);

    /// Update transmission state.
    /// @param throttle  Throttle position [0, 1]
    /// @param dt        Time step in seconds
    void update(float throttle, float dt);

    /// Get current RPM (output after transmission logic).
    float rpm() const { return rpm_; }

    /// Get current gear (1-based, 0 = neutral).
    uint8_t gear() const { return gear_; }

    /// Get current engine load [0, 1].
    float load() const { return load_; }

    /// Manual shift up.
    void shift_up();

    /// Manual shift down.
    void shift_down();

    /// Reset to idle in first gear.
    void reset();

private:
    Config config_;
    float rpm_ = 800.0f;
    float load_ = 0.0f;
    uint8_t gear_ = 1;
    float shift_timer_ = 0.0f;
    bool shifting_ = false;
};

} // namespace exhaust
