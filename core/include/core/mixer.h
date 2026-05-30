#pragma once

#include "core/types.h"

namespace exhaust {

/// Multi-channel audio mixer with soft clipping.
class Mixer {
public:
    Mixer() = default;

    /// Clear the internal mix buffer to silence.
    /// @param frames  Number of frames in this processing block
    void clear(size_t frames);

    /// Add a channel into the mix buffer with a given gain.
    /// @param input   Input samples to mix in
    /// @param frames  Number of frames
    /// @param gain    Linear gain factor (1.0 = unity)
    void add(const sample_t* input, size_t frames, float gain = 1.0f);

    /// Finalize the mix: apply soft clipping and write to output.
    /// @param output  Output buffer (16-bit PCM)
    /// @param frames  Number of frames
    void finalize(sample_t* output, size_t frames);

    /// Set master volume.
    /// @param volume  Linear gain (0.0 - 1.0+)
    void set_master_volume(float volume) { master_volume_ = volume; }

    float master_volume() const { return master_volume_; }

private:
    static constexpr size_t kMaxFrames = 2048;
    float mix_buffer_[kMaxFrames] = {};
    float master_volume_ = 0.8f;
};

} // namespace exhaust
