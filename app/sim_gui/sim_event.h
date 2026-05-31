#pragma once

#include <cstdint>

namespace exhaust {

/// Platform-agnostic key codes (subset relevant to the simulator).
enum class Key : uint8_t {
    Unknown = 0,
    W,
    A,
    S,
    D,
    E,
    Q,
    Up,
    Down,
    Left,
    Right,
    Escape,
    Space,
};

/// Platform-agnostic input event.
struct SimEvent {
    enum class Type : uint8_t {
        None = 0,
        KeyDown,
        KeyUp,
        Quit,
    };

    Type type = Type::None;
    Key key = Key::Unknown;
    bool repeat = false; ///< True if this is a key-repeat event
};

} // namespace exhaust
