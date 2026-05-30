#pragma once

#include <cstddef>
#include <cstdint>

namespace exhaust {

/// Audio sample type (16-bit signed PCM)
using sample_t = int16_t;

/// Maximum number of RPM layers per load state
constexpr size_t kMaxLayers = 8;

/// Maximum number of extra sound channels (turbo, backfire, horn, etc.)
constexpr size_t kMaxExtraChannels = 4;

/// Default audio sample rate
constexpr uint32_t kDefaultSampleRate = 44100;

/// Default audio buffer size in frames
constexpr uint16_t kDefaultBufferFrames = 512;

} // namespace exhaust
