#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Audio buffer callback type.
/// Called from DMA ISR to request new audio data.
/// @param buffer  Buffer to fill with 16-bit PCM samples
/// @param frames  Number of frames (samples) to fill
typedef void (*audio_fill_callback_t)(int16_t* buffer, uint32_t frames);

/// Initialize the audio subsystem (WM8988 + I2S + DMA).
/// @param callback  Function to call when buffer needs filling
void audio_i2s_init(audio_fill_callback_t callback);

/// Start audio playback.
void audio_i2s_start(void);

/// Stop audio playback.
void audio_i2s_stop(void);

/// Set WM8988 output volume (0-255).
void audio_i2s_set_volume(uint8_t volume);

#ifdef __cplusplus
}
#endif
