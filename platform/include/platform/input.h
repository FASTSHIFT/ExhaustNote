#pragma once

#include <cstdint>

namespace exhaust {

/// Input state from controller (keyboard, RC receiver, etc.)
struct InputState {
    float throttle = 0.0f; ///< [0, 1]
    bool shift_up = false; ///< Rising edge: shift up
    bool shift_down = false; ///< Rising edge: shift down
    bool engine_toggle = false; ///< Rising edge: start/stop engine
    bool quit = false; ///< Request to exit
};

/// Abstract input interface.
class IInput {
public:
    virtual ~IInput() = default;

    /// Initialize the input system.
    virtual bool init() = 0;

    /// Poll current input state (non-blocking).
    virtual InputState poll() = 0;
};

} // namespace exhaust
