#include "core/engine_voice.h"
#include "core/mixer.h"
#include "core/transmission.h"
#include "core/types.h"
#include "platform/audio_output.h"
#include "platform/input.h"

#include <SDL2/SDL.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>

// Forward declarations from platform/sim
namespace exhaust {
std::unique_ptr<IAudioOutput> create_audio_output();
std::unique_ptr<IInput> create_input();
}

/// Generate a synthetic engine sample layer (sine-based harmonics).
/// This creates a test tone that simulates an engine at a given RPM.
static void generate_synthetic_layer(exhaust::sample_t* buffer, size_t length,
    float rpm, uint8_t cylinders, uint32_t sample_rate)
{
    float fundamental = rpm * static_cast<float>(cylinders) / 120.0f;
    float phase = 0.0f;
    float phase_inc = fundamental / static_cast<float>(sample_rate);

    for (size_t i = 0; i < length; ++i) {
        // Fundamental + harmonics for a richer sound
        float sample = 0.0f;
        sample += 0.5f * std::sin(2.0f * 3.14159265f * phase); // 1st harmonic
        sample += 0.3f * std::sin(2.0f * 3.14159265f * phase * 2.0f); // 2nd
        sample += 0.15f * std::sin(2.0f * 3.14159265f * phase * 3.0f); // 3rd
        sample += 0.05f * std::sin(2.0f * 3.14159265f * phase * 4.0f); // 4th

        buffer[i] = static_cast<exhaust::sample_t>(sample * 20000.0f);
        phase += phase_inc;
        if (phase >= 1.0f)
            phase -= 1.0f;
    }
}

int main(int /* argc */, char* /* argv */[])
{
    using namespace exhaust;

    std::printf("ExhaustNote Simulator v0.1\n");
    std::printf("Controls: W/Up=throttle, E=engine on/off, D/Right=shift up, A/Left=shift down, Q/Esc=quit\n\n");

    // Initialize SDL
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS) < 0) {
        std::fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return 1;
    }

    // Create platform components
    auto audio = create_audio_output();
    auto input = create_input();

    if (!audio->init(kDefaultSampleRate, kDefaultBufferFrames)) {
        std::fprintf(stderr, "Audio init failed\n");
        return 1;
    }
    input->init();

    // --- Generate synthetic test layers ---
    constexpr uint8_t kCylinders = 4;
    constexpr size_t kLayerLength = kDefaultSampleRate; // 1 second per layer

    // 6 RPM layers for onload
    float layer_rpms[] = { 1000.0f, 2000.0f, 3500.0f, 5000.0f, 6500.0f, 8000.0f };
    constexpr uint8_t kNumLayers = 6;

    // Allocate and generate synthetic samples
    static sample_t layer_data[kNumLayers][kLayerLength];
    for (uint8_t i = 0; i < kNumLayers; ++i) {
        generate_synthetic_layer(layer_data[i], kLayerLength, layer_rpms[i], kCylinders, kDefaultSampleRate);
    }

    // Configure engine voice
    EngineVoice::LayerConfig onload_configs[kNumLayers];
    for (uint8_t i = 0; i < kNumLayers; ++i) {
        onload_configs[i] = { layer_rpms[i], layer_data[i], kLayerLength };
    }

    // Use same layers for offload (in real use, these would be different recordings)
    EngineVoice engine;
    engine.set_onload_layers(onload_configs, kNumLayers);
    engine.set_offload_layers(onload_configs, kNumLayers);
    engine.set_engine_config(kCylinders, kDefaultSampleRate);

    // Transmission
    Transmission::Config trans_config;
    trans_config.rpm_idle = 800.0f;
    trans_config.rpm_redline = 8500.0f;
    Transmission transmission(trans_config);

    // Mixer
    Mixer mixer;

    // Shared state between audio thread and main thread
    std::mutex state_mutex;
    EngineVoice::Params engine_params;
    std::atomic<bool> engine_running { true };

    // Audio callback
    audio->start([&](sample_t* output, size_t frames) {
        std::lock_guard<std::mutex> lock(state_mutex);
        if (!engine_running.load()) {
            std::memset(output, 0, frames * sizeof(sample_t));
            return;
        }
        mixer.clear(frames);
        sample_t engine_buf[kDefaultBufferFrames];
        engine.process(engine_params, engine_buf, frames);
        mixer.add(engine_buf, frames, 1.0f);
        mixer.finalize(output, frames);
    });

    std::printf("[Engine ON] RPM: %.0f  Gear: %d  Throttle: %.0f%%\n",
        transmission.rpm(), transmission.gear(), 0.0f);

    // Main loop
    bool running = true;
    Uint32 last_tick = SDL_GetTicks();

    while (running) {
        Uint32 now = SDL_GetTicks();
        float dt = static_cast<float>(now - last_tick) / 1000.0f;
        last_tick = now;

        // Clamp dt to avoid huge jumps
        if (dt > 0.1f)
            dt = 0.1f;

        // Poll input
        InputState inp = input->poll();
        if (inp.quit) {
            running = false;
            break;
        }
        if (inp.engine_toggle) {
            bool was_running = engine_running.load();
            engine_running.store(!was_running);
            std::printf("[Engine %s]\n", was_running ? "OFF" : "ON");
        }
        if (inp.shift_up)
            transmission.shift_up();
        if (inp.shift_down)
            transmission.shift_down();

        // Update transmission
        transmission.update(inp.throttle, dt);

        // Update engine params (thread-safe)
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            engine_params.rpm = transmission.rpm();
            engine_params.throttle = inp.throttle;
            engine_params.load = transmission.load();
        }

        // Print status
        std::printf("\r  RPM: %5.0f | Gear: %d | Throttle: %3.0f%% | Load: %.2f  ",
            transmission.rpm(), transmission.gear(),
            inp.throttle * 100.0f, transmission.load());
        std::fflush(stdout);

        SDL_Delay(16); // ~60 FPS update rate
    }

    audio->stop();
    SDL_Quit();
    std::printf("\nExhaustNote stopped.\n");
    return 0;
}
