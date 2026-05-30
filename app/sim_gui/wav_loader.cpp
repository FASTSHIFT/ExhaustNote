#include "wav_loader.h"

#include <cstdio>
#include <cstring>

namespace exhaust {

#pragma pack(push, 1)
struct WavHeader {
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
};
#pragma pack(pop)

bool load_wav_mono(const std::string& path, std::vector<sample_t>& out)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        std::fprintf(stderr, "Cannot open: %s\n", path.c_str());
        return false;
    }

    WavHeader header;
    if (std::fread(&header, sizeof(header), 1, f) != 1) {
        std::fclose(f);
        return false;
    }

    if (std::memcmp(header.riff, "RIFF", 4) != 0 || std::memcmp(header.wave, "WAVE", 4) != 0) {
        std::fprintf(stderr, "Not a WAV file: %s\n", path.c_str());
        std::fclose(f);
        return false;
    }

    // Skip to data chunk
    char chunk_id[4];
    uint32_t chunk_size;
    // Skip extra fmt bytes if any
    long data_start = sizeof(WavHeader);
    if (header.fmt_size > 16) {
        data_start += (header.fmt_size - 16);
    }
    std::fseek(f, static_cast<long>(12 + 8 + header.fmt_size), SEEK_SET);

    // Find "data" chunk
    while (std::fread(chunk_id, 4, 1, f) == 1) {
        if (std::fread(&chunk_size, 4, 1, f) != 1)
            break;
        if (std::memcmp(chunk_id, "data", 4) == 0)
            break;
        std::fseek(f, static_cast<long>(chunk_size), SEEK_CUR);
    }

    if (std::memcmp(chunk_id, "data", 4) != 0) {
        std::fprintf(stderr, "No data chunk in: %s\n", path.c_str());
        std::fclose(f);
        return false;
    }

    uint16_t channels = header.channels;
    uint16_t bits = header.bits_per_sample;
    size_t num_frames = chunk_size / (channels * (bits / 8));

    if (bits != 16) {
        std::fprintf(stderr, "Only 16-bit WAV supported, got %d-bit: %s\n", bits, path.c_str());
        std::fclose(f);
        return false;
    }

    // Read raw data
    std::vector<int16_t> raw(num_frames * channels);
    std::fread(raw.data(), sizeof(int16_t), num_frames * channels, f);
    std::fclose(f);

    // Convert to mono
    out.resize(num_frames);
    if (channels == 1) {
        std::memcpy(out.data(), raw.data(), num_frames * sizeof(int16_t));
    } else {
        // Downmix stereo to mono
        for (size_t i = 0; i < num_frames; ++i) {
            int32_t sum = 0;
            for (uint16_t ch = 0; ch < channels; ++ch) {
                sum += raw[i * channels + ch];
            }
            out[i] = static_cast<int16_t>(sum / channels);
        }
    }

    return true;
}

} // namespace exhaust
