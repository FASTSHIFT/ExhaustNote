#pragma once

#include "sim_state.h"

#include "core/transmission.h"

namespace exhaust {

/// Apply physics config to transmission, step simulation, read back results.
/// This is the ONLY place that calls Transmission setters — prevents conflicts.
void sim_physics_update(SimState& state, Transmission& trans, float dt);

} // namespace exhaust
