#pragma once

#include "core/types.h"
#include <cstddef>
#include <string>
#include <vector>

namespace exhaust {

/// Load a WAV file into a mono int16 buffer.
/// Stereo files are downmixed to mono.
/// @param path  Path to the WAV file
/// @param out   Output vector (resized to fit)
/// @return true on success
bool load_wav_mono(const std::string& path, std::vector<sample_t>& out);

} // namespace exhaust
