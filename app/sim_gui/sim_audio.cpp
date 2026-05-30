#include "sim_audio.h"
#include "wav_loader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace exhaust {

// Persistent sample data (must outlive AudioState references)
static std::vector<sample_t> g_onload_data[8];
static std::vector<sample_t> g_offload_data[8];
static CarConfig g_car_config;

const CarConfig& current_car_config()
{
    return g_car_config;
}

void generate_combustion_pulse(sample_t* buffer, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        float t = static_cast<float>(i) / 44100.0f;
        float env = std::exp(-t * 150.0f);
        float wave = std::sin(t * 120.0f * 6.28f) * 0.6f
            + std::sin(t * 60.0f * 6.28f) * 0.4f;
        buffer[i] = static_cast<sample_t>(wave * env * 12000.0f);
    }
}

bool load_backfire_sample(const std::string& path, AudioState& audio_state)
{
    if (!load_wav_mono(path, audio_state.backfire_sample)) {
        std::printf("  No backfire sample (optional)\n");
        return false;
    }
    std::printf("  Backfire sample loaded (%.1fs)\n",
        audio_state.backfire_sample.size() / 44100.0f);
    return true;
}

bool load_car(const std::string& json_path,
    AudioState& audio_state, Transmission& transmission,
    const sample_t* pulse_sample, size_t pulse_len)
{
    CarConfig cfg;
    if (!load_car_config(json_path, cfg))
        return false;

    std::printf("\nLoading: %s (%s)\n", cfg.name.c_str(), cfg.engine_type.c_str());

    // Load onload WAVs
    for (size_t i = 0; i < cfg.onload.size() && i < 8; ++i) {
        std::string path = cfg.base_dir + "/" + cfg.onload[i].file;
        if (!load_wav_mono(path, g_onload_data[i])) {
            std::fprintf(stderr, "  WARN: %s\n", path.c_str());
            g_onload_data[i].resize(44100, 0);
        } else {
            std::printf("  on[%zu]: %s (%.1fs)\n", i, cfg.onload[i].file.c_str(),
                g_onload_data[i].size() / 44100.0f);
        }
    }

    // Load offload WAVs
    for (size_t i = 0; i < cfg.offload.size() && i < 8; ++i) {
        std::string path = cfg.base_dir + "/" + cfg.offload[i].file;
        if (!load_wav_mono(path, g_offload_data[i])) {
            g_offload_data[i].resize(44100, 0);
        } else {
            std::printf("  off[%zu]: %s (%.1fs)\n", i, cfg.offload[i].file.c_str(),
                g_offload_data[i].size() / 44100.0f);
        }
    }

    // Configure engine voice
    {
        std::lock_guard<std::mutex> lock(audio_state.mutex);

        EngineVoice::LayerConfig onload_configs[8];
        for (size_t i = 0; i < cfg.onload.size() && i < 8; ++i) {
            onload_configs[i].rpm = cfg.onload[i].rpm;
            onload_configs[i].data = g_onload_data[i].data();
            onload_configs[i].length = g_onload_data[i].size();
        }
        EngineVoice::LayerConfig offload_configs[8];
        for (size_t i = 0; i < cfg.offload.size() && i < 8; ++i) {
            offload_configs[i].rpm = cfg.offload[i].rpm;
            offload_configs[i].data = g_offload_data[i].data();
            offload_configs[i].length = g_offload_data[i].size();
        }

        audio_state.engine.set_onload_layers_auto(onload_configs,
            static_cast<uint8_t>(std::min(cfg.onload.size(), size_t(8))));
        audio_state.engine.set_offload_layers_auto(offload_configs,
            static_cast<uint8_t>(std::min(cfg.offload.size(), size_t(8))));
        audio_state.engine.set_engine_config(cfg.cylinders, kDefaultSampleRate);
        audio_state.engine.reset();

        // Update combustion pulse
        CombustionPulse::Config pc;
        pc.cylinders = cfg.cylinders;
        pc.volume = 0.2f;
        pc.rpm_fade_start = 3000.0f;
        pc.rpm_fade_end = 5000.0f;
        audio_state.combustion.load_pulse(pulse_sample, pulse_len);
        audio_state.combustion.set_config(pc);
    }

    // Configure transmission
    Transmission::Config tc;
    tc.num_gears = static_cast<uint8_t>(std::min(cfg.gear_ratios.size(), size_t(8)));
    for (size_t i = 0; i < cfg.gear_ratios.size() && i < 8; ++i)
        tc.gear_ratios[i] = cfg.gear_ratios[i];
    tc.final_drive = cfg.final_drive;
    tc.rpm_idle = cfg.rpm_idle;
    tc.rpm_redline = cfg.rpm_redline;
    tc.peak_torque = cfg.peak_torque;
    tc.peak_torque_rpm = cfg.peak_torque_rpm;
    tc.inertia = cfg.inertia;
    tc.rpm_upshift = 2.0f; // manual mode
    tc.rpm_downshift = -1.0f;
    transmission = Transmission(tc);

    g_car_config = cfg;
    std::printf("  Ready: %d cyl, %d gears, redline %d\n",
        cfg.cylinders, tc.num_gears, (int)cfg.rpm_redline);
    return true;
}

void audio_callback(void* userdata, Uint8* stream, int len)
{
    auto* state = static_cast<AudioState*>(userdata);
    size_t frames = static_cast<size_t>(len) / sizeof(sample_t);

    std::lock_guard<std::mutex> lock(state->mutex);
    if (!state->engine_running) {
        std::memset(stream, 0, static_cast<size_t>(len));
        return;
    }

    sample_t engine_buf[2048];
    float effects_buf[2048];
    frames = std::min(frames, size_t(2048));

    // Apply idle fluctuation to RPM
    EngineVoice::Params params = state->params;
    params.rpm += state->idle_fluct.update(params.rpm, state->dt);
    if (params.rpm < 500.0f)
        params.rpm = 500.0f;

    // Process main engine voice
    state->engine.process(params, engine_buf, frames);

    // Apply throttle transient gain
    float transient_gain = state->transient.update(state->throttle, state->dt);

    // Add combustion pulses
    std::memset(effects_buf, 0, frames * sizeof(float));
    state->combustion.process(effects_buf, frames, params.rpm, 44100);

    // Mix everything
    state->mixer.clear(frames);
    state->mixer.add(engine_buf, frames, transient_gain);

    sample_t pulse_buf[2048];
    for (size_t i = 0; i < frames; ++i) {
        float s = effects_buf[i];
        s = std::fmax(-32768.0f, std::fmin(32767.0f, s));
        pulse_buf[i] = static_cast<sample_t>(s);
    }
    state->mixer.add(pulse_buf, frames, 1.0f);

    // Backfire one-shot playback
    if (state->afterfire_level > 0.3f && !state->backfire_playing
        && !state->backfire_sample.empty()) {
        state->backfire_playing = true;
        state->backfire_pos = 0;
    }
    if (state->backfire_playing && !state->backfire_sample.empty()) {
        sample_t bf_buf[2048] = {};
        size_t bf_len = state->backfire_sample.size();
        for (size_t i = 0; i < frames && state->backfire_pos < bf_len; ++i) {
            bf_buf[i] = state->backfire_sample[state->backfire_pos++];
        }
        if (state->backfire_pos >= bf_len) {
            state->backfire_playing = false;
        }
        state->mixer.add(bf_buf, frames, 0.6f);
    }

    state->mixer.finalize(reinterpret_cast<sample_t*>(stream), frames);
}

} // namespace exhaust
