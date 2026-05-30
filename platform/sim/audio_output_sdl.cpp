#include "platform/audio_output.h"

#include <SDL2/SDL.h>
#include <cstdio>
#include <memory>

namespace exhaust {

class SdlAudioOutput : public IAudioOutput {
public:
    ~SdlAudioOutput() override
    {
        stop();
        if (device_id_ != 0) {
            SDL_CloseAudioDevice(device_id_);
        }
    }

    bool init(uint32_t sample_rate, uint16_t buffer_frames) override
    {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            std::fprintf(stderr, "SDL audio init failed: %s\n", SDL_GetError());
            return false;
        }

        SDL_AudioSpec desired = {};
        desired.freq = static_cast<int>(sample_rate);
        desired.format = AUDIO_S16SYS;
        desired.channels = 1;
        desired.samples = buffer_frames;
        desired.callback = audio_callback;
        desired.userdata = this;

        SDL_AudioSpec obtained = {};
        device_id_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
        if (device_id_ == 0) {
            std::fprintf(stderr, "SDL open audio device failed: %s\n", SDL_GetError());
            return false;
        }

        return true;
    }

    void start(AudioCallback callback) override
    {
        callback_ = std::move(callback);
        running_ = true;
        SDL_PauseAudioDevice(device_id_, 0);
    }

    void stop() override
    {
        if (device_id_ != 0) {
            SDL_PauseAudioDevice(device_id_, 1);
        }
        running_ = false;
    }

    bool is_running() const override { return running_; }

private:
    static void audio_callback(void* userdata, Uint8* stream, int len)
    {
        auto* self = static_cast<SdlAudioOutput*>(userdata);
        size_t frames = static_cast<size_t>(len) / sizeof(sample_t);
        if (self->callback_) {
            self->callback_(reinterpret_cast<sample_t*>(stream), frames);
        } else {
            SDL_memset(stream, 0, static_cast<size_t>(len));
        }
    }

    SDL_AudioDeviceID device_id_ = 0;
    AudioCallback callback_;
    bool running_ = false;
};

/// Factory function to create SDL audio output.
std::unique_ptr<IAudioOutput> create_audio_output()
{
    return std::make_unique<SdlAudioOutput>();
}

} // namespace exhaust
