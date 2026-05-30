#pragma once

#include "platform/audio_output.h"
#include "platform/input.h"
#include <memory>

namespace exhaust {

/// Create the platform-specific audio output (SDL2 implementation).
std::unique_ptr<IAudioOutput> create_audio_output();

/// Create the platform-specific input (keyboard implementation).
std::unique_ptr<IInput> create_input();

} // namespace exhaust
