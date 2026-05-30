#pragma once

#include "core/types.h"
#include <functional>

namespace exhaust {

/// Audio callback type: called by the audio backend to request samples.
/// @param output  Buffer to fill with audio samples
/// @param frames  Number of frames requested
using AudioCallback = std::function<void(sample_t* output, size_t frames)>;

/// Abstract audio output interface.
class IAudioOutput {
public:
    virtual ~IAudioOutput() = default;

    /// Initialize the audio output.
    /// @param sample_rate   Sample rate in Hz
    /// @param buffer_frames Buffer size in frames
    /// @return true on success
    virtual bool init(uint32_t sample_rate, uint16_t buffer_frames) = 0;

    /// Start audio playback with the given callback.
    virtual void start(AudioCallback callback) = 0;

    /// Stop audio playback.
    virtual void stop() = 0;

    /// Check if audio is currently playing.
    virtual bool is_running() const = 0;
};

} // namespace exhaust
