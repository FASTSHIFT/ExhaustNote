#pragma once

#include "sim_state.h"

#include <string>
#include <vector>

namespace exhaust {

/// Render the Controls window (car selector, engine, gear, throttle, physics sliders, volume).
/// Reads state for display; writes state.physics.*, state.car_switch_*, state.throttle.
void sim_gui_controls(SimState& state,
    const std::vector<std::pair<std::string, std::string>>& car_list,
    float& master_volume);

/// Render the Telemetry window (RPM, throttle/load, speed plots).
/// Pure read-only from state.
void sim_gui_telemetry(const SimState& state);

} // namespace exhaust
