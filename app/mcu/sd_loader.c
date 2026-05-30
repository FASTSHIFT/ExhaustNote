/**
 * SD Card + FatFS + WAV Loader implementation
 */
#include "sd_loader.h"

#include "ff.h"

#include <string.h>

static FATFS g_fatfs;
static uint8_t g_mounted = 0;

int sd_mount(void)
{
    FRESULT res = f_mount(&g_fatfs, "1:", 1);
    if (res != FR_OK) {
        return -(int)res;
    }
    g_mounted = 1;
    return 0;
}

void sd_unmount(void)
{
    if (g_mounted) {
        f_mount(NULL, "1:", 0);
        g_mounted = 0;
    }
}

// WAV header structure (packed)
#pragma pack(push, 1)
typedef struct {
    char riff[4];
    uint32_t file_size;
    char wave[4];
    char fmt[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} wav_header_t;
#pragma pack(pop)

uint32_t sd_wav_load(const char* path, int16_t* buf, uint32_t buf_max, sd_wav_info_t* info)
{
    FIL file;
    UINT bytes_read;
    wav_header_t header;

    if (f_open(&file, path, FA_READ) != FR_OK) {
        return 0;
    }

    // Read WAV header
    if (f_read(&file, &header, sizeof(header), &bytes_read) != FR_OK
        || bytes_read != sizeof(header)) {
        f_close(&file);
        return 0;
    }

    // Validate
    if (memcmp(header.riff, "RIFF", 4) != 0 || memcmp(header.wave, "WAVE", 4) != 0) {
        f_close(&file);
        return 0;
    }

    // Only support 16-bit PCM
    if (header.audio_format != 1 || header.bits_per_sample != 16) {
        f_close(&file);
        return 0;
    }

    // Skip extra fmt bytes and find "data" chunk
    uint32_t seek_pos = 12 + 8 + header.fmt_size;
    f_lseek(&file, seek_pos);

    char chunk_id[4];
    uint32_t chunk_size = 0;

    while (f_read(&file, chunk_id, 4, &bytes_read) == FR_OK && bytes_read == 4) {
        if (f_read(&file, &chunk_size, 4, &bytes_read) != FR_OK || bytes_read != 4) {
            break;
        }
        if (memcmp(chunk_id, "data", 4) == 0) {
            break;
        }
        f_lseek(&file, f_tell(&file) + chunk_size);
    }

    if (memcmp(chunk_id, "data", 4) != 0) {
        f_close(&file);
        return 0;
    }

    uint16_t channels = header.channels;
    uint32_t num_frames = chunk_size / (channels * 2);

    if (info) {
        info->sample_rate = header.sample_rate;
        info->channels = channels;
        info->bits_per_sample = header.bits_per_sample;
        info->num_frames = num_frames;
    }

    if (num_frames > buf_max) {
        num_frames = buf_max;
    }

    if (channels == 1) {
        // Mono: read directly
        uint32_t bytes_to_read = num_frames * 2;
        if (f_read(&file, buf, bytes_to_read, &bytes_read) != FR_OK) {
            f_close(&file);
            return 0;
        }
        num_frames = bytes_read / 2;
    } else {
        // Stereo: take left channel
        uint32_t frames_loaded = 0;
        int16_t stereo_buf[256];

        while (frames_loaded < num_frames) {
            uint32_t chunk_frames = num_frames - frames_loaded;
            if (chunk_frames > 128)
                chunk_frames = 128;

            uint32_t bytes_to_read = chunk_frames * channels * 2;
            if (f_read(&file, stereo_buf, bytes_to_read, &bytes_read) != FR_OK
                || bytes_read == 0) {
                break;
            }

            uint32_t frames_got = bytes_read / (channels * 2);
            for (uint32_t i = 0; i < frames_got; i++) {
                buf[frames_loaded + i] = stereo_buf[i * channels];
            }
            frames_loaded += frames_got;
        }
        num_frames = frames_loaded;
    }

    f_close(&file);
    return num_frames;
}

uint32_t sd_read_file(const char* path, char* buf, uint32_t buf_size)
{
    FIL file;
    UINT bytes_read;

    if (f_open(&file, path, FA_READ) != FR_OK) {
        return 0;
    }

    uint32_t fsize = f_size(&file);
    if (fsize >= buf_size) {
        fsize = buf_size - 1;
    }

    if (f_read(&file, buf, fsize, &bytes_read) != FR_OK) {
        f_close(&file);
        return 0;
    }

    buf[bytes_read] = '\0';
    f_close(&file);
    return bytes_read;
}

uint8_t sd_scan_cars(const char* base_dir, sd_car_entry_t* entries, uint8_t max_entries)
{
    DIR dir;
    FILINFO fno;
    FIL test_file;
    uint8_t count = 0;
    char path_buf[128];

    if (f_opendir(&dir, base_dir) != FR_OK) {
        return 0;
    }

    while (count < max_entries) {
        if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) {
            break; // End of directory
        }

        // Skip non-directories and hidden/system entries
        if (!(fno.fattrib & AM_DIR)) {
            continue;
        }
        if (fno.fname[0] == '.') {
            continue;
        }

        // Check if this directory contains car.json
        snprintf(path_buf, sizeof(path_buf), "%s/%s/car.json", base_dir, fno.fname);
        if (f_open(&test_file, path_buf, FA_READ) == FR_OK) {
            f_close(&test_file);
            // Valid car directory
            strncpy(entries[count].name, fno.fname, sizeof(entries[count].name) - 1);
            entries[count].name[sizeof(entries[count].name) - 1] = '\0';
            count++;
        }
    }

    f_closedir(&dir);
    return count;
}
