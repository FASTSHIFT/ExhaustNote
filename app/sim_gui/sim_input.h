#pragma once

#include "sim_state.h"

#include <SDL2/SDL.h>

namespace exhaust {

/// Update SimState from keyboard input (continuous keys).
/// Handles: W/Up = throttle up, S/Down = brake, throttle decay.
/// Discrete keys (E, D, A, Q) are handled in the event loop.
void sim_input_update(SimState& state, const Uint8* keys, float dt);

} // namespace exhaust
