/**
 * SD Card + FatFS + WAV Loader for MCU
 *
 * Responsibilities:
 * - Mount SD card via FatFS
 * - Read WAV files into caller-provided buffers (SDRAM)
 * - Parse WAV header, return mono 16-bit PCM data
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Mount the SD card filesystem.
/// @return 0 on success, negative on error
int sd_mount(void);

/// Unmount the SD card filesystem.
void sd_unmount(void);

/// WAV file info returned by sd_wav_load.
typedef struct {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint32_t num_frames; ///< Total frames (samples per channel)
} sd_wav_info_t;

/// Load a WAV file from SD card into a pre-allocated buffer.
/// Converts stereo to mono (left channel) if needed.
/// @param path       FatFS path (e.g. "1:/ExhaustNote/ferrari_458/ext_on3500.wav")
/// @param buf        Output buffer for mono int16 PCM samples
/// @param buf_max    Maximum number of samples the buffer can hold
/// @param info       [out] WAV file info (sample rate, etc.)
/// @return Number of samples loaded, or 0 on error
uint32_t sd_wav_load(const char* path, int16_t* buf, uint32_t buf_max, sd_wav_info_t* info);

/// Read a text file from SD card into a buffer (for JSON config).
/// @param path       FatFS path
/// @param buf        Output buffer (will be null-terminated)
/// @param buf_size   Buffer size in bytes
/// @return Number of bytes read, or 0 on error
uint32_t sd_read_file(const char* path, char* buf, uint32_t buf_size);

#ifdef __cplusplus
}
#endif
