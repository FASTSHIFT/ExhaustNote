#pragma once

#include <cstdint>
#include <cstring>

namespace exhaust {

// --- History buffer for plotting ---
struct PlotHistory {
    static constexpr int kMaxSize = 600;
    float data[kMaxSize] = {};
    int count = 0;
    void push(float val)
    {
        if (count < kMaxSize)
            data[count++] = val;
        else {
            std::memmove(data, data + 1, (kMaxSize - 1) * sizeof(float));
            data[kMaxSize - 1] = val;
        }
    }
};

/// Physics tuning parameters (persist across car switches).
struct PhysicsConfig {
    float load_nm = 200.0f;
    float engine_brake_nm = 60.0f;
    float road_coeff = 0.3f;
    float brake_force_nm = 400.0f;
};

/// All simulator state in one place — single source of truth.
struct SimState {
    // --- Input ---
    float throttle = 0.0f;
    bool braking = false;
    bool engine_on = false;

    // --- Physics config (GUI-adjustable, persists across car switch) ---
    PhysicsConfig physics;

    // --- Derived display state (written by physics, read by GUI) ---
    float smoothed_rpm = 900.0f;
    float speed_kmh = 0.0f;
    uint8_t gear = 1;
    float load = 0.0f;
    bool rev_limiter = false;
    float afterfire = 0.0f;

    // --- Car management ---
    int current_car = 0;
    bool car_switch_requested = false;
    int car_switch_target = -1;

    // --- Plot history ---
    PlotHistory rpm_history;
    PlotHistory throttle_history;
    PlotHistory load_history;
    PlotHistory speed_history;
};

} // namespace exhaust
