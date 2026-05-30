#pragma once

#include "core/types.h"

namespace exhaust {

/// Result of crossfade computation between two adjacent layers
struct CrossfadeResult {
    uint8_t layer_lo; ///< Index of the lower RPM layer
    uint8_t layer_hi; ///< Index of the higher RPM layer
    float mix; ///< Blend factor: 0.0 = all low, 1.0 = all high
};

/// Compute which two layers to blend and the mix ratio for a given RPM.
///
/// @param rpm         Current RPM value (actual, not normalized)
/// @param layer_rpms  Array of RPM values for each layer (sorted ascending)
/// @param num_layers  Number of layers (must be >= 1)
/// @return CrossfadeResult with layer indices and mix factor
CrossfadeResult compute_crossfade(float rpm, const float* layer_rpms, uint8_t num_layers);

/// Mix two sample buffers with a crossfade ratio.
///
/// output[i] = lo[i] * (1 - mix) + hi[i] * mix
///
/// @param lo      Lower layer samples
/// @param hi      Higher layer samples
/// @param output  Output buffer (may alias lo or hi)
/// @param frames  Number of frames to process
/// @param mix     Blend factor [0.0, 1.0]
void mix_layers(const sample_t* lo, const sample_t* hi, sample_t* output,
    size_t frames, float mix);

} // namespace exhaust
