#include "core/engine_effects.h"
#include "core/engine_voice.h"
#include "core/mixer.h"
#include "core/transmission.h"
#include "core/types.h"
#include "wav_loader.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "implot.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

using namespace exhaust;

// --- Configuration ---
static const char* SAMPLE_DIR = "/tmp/ac_ferrari_458";

struct LayerDef {
    const char* filename;
    float rpm;
};

static const LayerDef kOnloadLayers[] = {
    { "F4CH_IDLE_EXT.wav", 900.0f },
    { "ext_on3500.wav", 3500.0f },
    { "ext_on5000.wav", 5000.0f },
    { "ext_on6750.wav", 6750.0f },
    { "ext_on8500.wav", 8500.0f },
};

static const LayerDef kOffloadLayers[] = {
    { "F4CH_IDLE_EXT.wav", 900.0f },
    { "ext_off3000.wav", 3000.0f },
    { "ext_off5000.wav", 5000.0f },
    { "ext_off7000.wav", 7000.0f },
    { "ext_off8500.wav", 8500.0f },
};

constexpr int kNumOnload = sizeof(kOnloadLayers) / sizeof(kOnloadLayers[0]);
constexpr int kNumOffload = sizeof(kOffloadLayers) / sizeof(kOffloadLayers[0]);
constexpr uint8_t kCylinders = 8; // Ferrari 458 is a V8

// --- History buffer for plotting (scrolling) ---
struct PlotHistory {
    static constexpr int kMaxSize = 600; // 10 seconds at 60fps
    float data[kMaxSize] = {};
    int count = 0;

    void push(float val)
    {
        if (count < kMaxSize) {
            data[count++] = val;
        } else {
            std::memmove(data, data + 1, (kMaxSize - 1) * sizeof(float));
            data[kMaxSize - 1] = val;
        }
    }
};

// --- Audio state ---
struct AudioState {
    std::mutex mutex;
    EngineVoice engine;
    Mixer mixer;
    CombustionPulse combustion;
    ThrottleTransient transient;
    IdleFluctuation idle_fluct;
    EngineVoice::Params params;
    float throttle = 0.0f;
    float dt = 0.016f;
    bool engine_running = false;
};

static void audio_callback(void* userdata, Uint8* stream, int len)
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

    // Apply throttle transient gain (no rev limiter on audio — it's in physics now)
    float transient_gain = state->transient.update(state->throttle, state->dt);

    // Add combustion pulses
    std::memset(effects_buf, 0, frames * sizeof(float));
    state->combustion.process(effects_buf, frames, params.rpm, 44100);

    // Mix everything
    state->mixer.clear(frames);
    state->mixer.add(engine_buf, frames, transient_gain);
    // Add combustion pulses as sample_t
    sample_t pulse_buf[2048];
    for (size_t i = 0; i < frames; ++i) {
        float s = effects_buf[i];
        s = std::fmax(-32768.0f, std::fmin(32767.0f, s));
        pulse_buf[i] = static_cast<sample_t>(s);
    }
    state->mixer.add(pulse_buf, frames, 1.0f);
    state->mixer.finalize(reinterpret_cast<sample_t*>(stream), frames);
}

int main(int, char**)
{
    // --- Load samples ---
    std::printf("Loading Ferrari 458 samples from %s ...\n", SAMPLE_DIR);

    std::vector<sample_t> onload_data[kNumOnload];
    std::vector<sample_t> offload_data[kNumOffload];

    for (int i = 0; i < kNumOnload; ++i) {
        std::string path = std::string(SAMPLE_DIR) + "/" + kOnloadLayers[i].filename;
        if (!load_wav_mono(path, onload_data[i])) {
            std::fprintf(stderr, "FATAL: Cannot load %s\n", path.c_str());
            return 1;
        }
        std::printf("  onload[%d]: %s (%.1fs)\n", i, kOnloadLayers[i].filename,
            onload_data[i].size() / 44100.0f);
    }

    for (int i = 0; i < kNumOffload; ++i) {
        std::string path = std::string(SAMPLE_DIR) + "/" + kOffloadLayers[i].filename;
        if (!load_wav_mono(path, offload_data[i])) {
            std::fprintf(stderr, "FATAL: Cannot load %s\n", path.c_str());
            return 1;
        }
        std::printf("  offload[%d]: %s (%.1fs)\n", i, kOffloadLayers[i].filename,
            offload_data[i].size() / 44100.0f);
    }

    // --- Setup engine voice ---
    AudioState audio_state;

    EngineVoice::LayerConfig onload_configs[kNumOnload];
    for (int i = 0; i < kNumOnload; ++i) {
        onload_configs[i].rpm = kOnloadLayers[i].rpm;
        onload_configs[i].data = onload_data[i].data();
        onload_configs[i].length = onload_data[i].size();
    }
    EngineVoice::LayerConfig offload_configs[kNumOffload];
    for (int i = 0; i < kNumOffload; ++i) {
        offload_configs[i].rpm = kOffloadLayers[i].rpm;
        offload_configs[i].data = offload_data[i].data();
        offload_configs[i].length = offload_data[i].size();
    }

    // Use auto-envelope: generates trapezoidal volume + per-layer pitch ranges
    audio_state.engine.set_onload_layers_auto(onload_configs, kNumOnload);
    audio_state.engine.set_offload_layers_auto(offload_configs, kNumOffload);
    audio_state.engine.set_engine_config(kCylinders, kDefaultSampleRate);
    audio_state.mixer.set_master_volume(0.8f);

    // --- Engine Effects ---
    // Combustion pulse: low-frequency thud simulating cylinder firing
    // Real combustion impulse is a low "thump" (~100-150Hz), not a click
    static sample_t pulse_sample[512];
    for (int i = 0; i < 512; ++i) {
        float t = static_cast<float>(i) / 44100.0f;
        float env = std::exp(-t * 150.0f); // Slower decay (~7ms, more body)
        // Low frequency content only: 120Hz fundamental + 60Hz sub
        float wave = std::sin(t * 120.0f * 6.28f) * 0.6f
            + std::sin(t * 60.0f * 6.28f) * 0.4f;
        pulse_sample[i] = static_cast<sample_t>(wave * env * 12000.0f);
    }
    audio_state.combustion.load_pulse(pulse_sample, 512);
    CombustionPulse::Config pulse_config;
    pulse_config.cylinders = kCylinders;
    pulse_config.volume = 0.2f; // Subtle, not dominant
    pulse_config.rpm_fade_start = 3000.0f; // Start fading earlier
    pulse_config.rpm_fade_end = 5000.0f; // Gone by mid-range
    // V8 firing order: slight variation only
    pulse_config.cyl_volumes[0] = 1.0f;
    pulse_config.cyl_volumes[1] = 0.9f;
    pulse_config.cyl_volumes[2] = 1.0f;
    pulse_config.cyl_volumes[3] = 0.95f;
    pulse_config.cyl_volumes[4] = 1.0f;
    pulse_config.cyl_volumes[5] = 0.9f;
    pulse_config.cyl_volumes[6] = 1.0f;
    pulse_config.cyl_volumes[7] = 0.95f;
    audio_state.combustion.set_config(pulse_config);

    // Idle fluctuation
    IdleFluctuation::Config idle_config;
    idle_config.amplitude = 25.0f; // ±25 RPM
    idle_config.rate = 1.0f; // 1 Hz breathing
    idle_config.rpm_threshold = 1500.0f;
    audio_state.idle_fluct.set_config(idle_config);

    // Throttle transient
    ThrottleTransient::Config trans_fx_config;
    trans_fx_config.attack_gain = 1.25f;
    trans_fx_config.attack_time = 0.06f;
    trans_fx_config.decay_time = 0.15f;
    audio_state.transient.set_config(trans_fx_config);

    // Rev limiter is now handled in the physics model (Transmission class)
    // It cuts fuel (throttle=0) causing RPM to naturally oscillate at redline

    // --- Transmission ---
    Transmission::Config trans_config;
    // Ferrari 458 Italia 7-speed dual-clutch (Getrag)
    // Ratios from public spec sheets
    trans_config.num_gears = 7;
    trans_config.gear_ratios[0] = 3.08f;
    trans_config.gear_ratios[1] = 2.19f;
    trans_config.gear_ratios[2] = 1.63f;
    trans_config.gear_ratios[3] = 1.29f;
    trans_config.gear_ratios[4] = 1.03f;
    trans_config.gear_ratios[5] = 0.84f;
    trans_config.gear_ratios[6] = 0.69f;
    trans_config.final_drive = 4.44f;
    trans_config.rpm_idle = 900.0f;
    trans_config.rpm_redline = 9000.0f;
    trans_config.rpm_upshift = 2.0f; // >1.0 = never auto-upshift (manual mode)
    trans_config.rpm_downshift = -1.0f; // <0 = never auto-downshift (manual mode)
    Transmission transmission(trans_config);

    // --- Init SDL + OpenGL ---
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "SDL init error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow(
        "ExhaustNote - Ferrari 458 Italia",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // VSync

    // --- Init ImGui ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    // --- Init Audio ---
    SDL_AudioSpec desired = {};
    desired.freq = 44100;
    desired.format = AUDIO_S16SYS;
    desired.channels = 1;
    desired.samples = 512;
    desired.callback = audio_callback;
    desired.userdata = &audio_state;

    SDL_AudioSpec obtained;
    SDL_AudioDeviceID audio_dev = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (audio_dev == 0) {
        std::fprintf(stderr, "Audio open failed: %s\n", SDL_GetError());
        return 1;
    }

    // --- State ---
    float throttle = 0.0f;
    bool engine_on = false;
    PlotHistory rpm_history, throttle_history, load_history, gear_history;

    std::printf("\nReady! Use the GUI or keyboard:\n");
    std::printf("  W/Up = throttle, E = engine on/off\n");
    std::printf("  D/Right = shift up, A/Left = shift down\n");

    // --- Main loop ---
    bool running = true;
    Uint32 last_tick = SDL_GetTicks();

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                running = false;
            if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                switch (event.key.keysym.sym) {
                case SDLK_e:
                    engine_on = !engine_on;
                    if (engine_on) {
                        SDL_PauseAudioDevice(audio_dev, 0);
                        transmission.reset();
                    } else {
                        SDL_PauseAudioDevice(audio_dev, 1);
                    }
                    break;
                case SDLK_d:
                case SDLK_RIGHT:
                    transmission.shift_up();
                    break;
                case SDLK_a:
                case SDLK_LEFT:
                    transmission.shift_down();
                    break;
                case SDLK_ESCAPE:
                case SDLK_q:
                    running = false;
                    break;
                }
            }
        }

        // Timing
        Uint32 now = SDL_GetTicks();
        float dt = (now - last_tick) / 1000.0f;
        last_tick = now;
        if (dt > 0.1f)
            dt = 0.1f;

        // Keyboard throttle (continuous)
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) {
            throttle += 3.0f * dt;
            if (throttle > 1.0f)
                throttle = 1.0f;
        } else if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) {
            throttle -= 5.0f * dt;
            if (throttle < 0.0f)
                throttle = 0.0f;
        } else {
            throttle -= 2.0f * dt;
            if (throttle < 0.0f)
                throttle = 0.0f;
        }

        // Update transmission
        if (engine_on) {
            transmission.update(throttle, dt);
        }

        // Update audio params
        {
            std::lock_guard<std::mutex> lock(audio_state.mutex);
            audio_state.params.rpm = transmission.rpm();
            audio_state.params.throttle = throttle;
            audio_state.params.load = transmission.load();
            audio_state.throttle = throttle;
            audio_state.dt = dt;
            audio_state.engine_running = engine_on;
        }

        // Push to history
        rpm_history.push(transmission.rpm());
        throttle_history.push(throttle * 100.0f);
        load_history.push(transmission.load() * 100.0f);
        gear_history.push(static_cast<float>(transmission.gear()));

        // --- ImGui Frame ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Main window
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 700), ImGuiCond_FirstUseEver);
        ImGui::Begin("Ferrari 458 Italia - Controls");

        // Engine state
        ImVec4 color = engine_on ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        ImGui::TextColored(color, "Engine: %s", engine_on ? "RUNNING" : "OFF");
        ImGui::SameLine();
        if (ImGui::Button(engine_on ? "Stop [E]" : "Start [E]")) {
            engine_on = !engine_on;
            if (engine_on) {
                SDL_PauseAudioDevice(audio_dev, 0);
                transmission.reset();
            } else {
                SDL_PauseAudioDevice(audio_dev, 1);
            }
        }

        ImGui::Separator();

        // RPM gauge
        ImGui::Text("RPM");
        float rpm_frac = (transmission.rpm() - trans_config.rpm_idle) / (trans_config.rpm_redline - trans_config.rpm_idle);
        ImVec4 rpm_color = transmission.rev_limiter_active()
            ? ImVec4(1, 0.2f, 0.2f, 1)
            : (rpm_frac > 0.85f ? ImVec4(1, 0.6f, 0, 1) : ImVec4(0.2f, 0.8f, 0.2f, 1));
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, rpm_color);
        ImGui::ProgressBar(rpm_frac, ImVec2(-1, 30), "");
        ImGui::PopStyleColor();
        ImGui::Text("%.0f / %.0f RPM %s", transmission.rpm(), trans_config.rpm_redline,
            transmission.rev_limiter_active() ? "[LIMITER]" : "");
        if (transmission.afterfire() > 0.01f) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), " BACKFIRE %.0f%%",
                transmission.afterfire() * 100.0f);
        }

        ImGui::Spacing();

        // Gear display
        ImGui::Text("Gear: %d / %d", transmission.gear(), trans_config.num_gears);
        ImGui::SameLine(200);
        if (ImGui::Button("Down [A]"))
            transmission.shift_down();
        ImGui::SameLine();
        if (ImGui::Button("Up [D]"))
            transmission.shift_up();

        ImGui::Spacing();

        // Throttle slider (directly controls engine torque via physics model)
        ImGui::Text("Throttle [W/S or drag slider]");
        float throttle_pct = throttle * 100.0f;
        if (ImGui::SliderFloat("##throttle", &throttle_pct, 0.0f, 100.0f, "%.0f%%")) {
            throttle = throttle_pct / 100.0f;
        }

        ImGui::Spacing();

        // Volume
        float vol_pct = audio_state.mixer.master_volume() * 100.0f;
        if (ImGui::SliderFloat("Volume", &vol_pct, 0.0f, 100.0f, "%.0f%%")) {
            audio_state.mixer.set_master_volume(vol_pct / 100.0f);
        }

        ImGui::Separator();
        ImGui::Text("Keyboard: W=gas S=brake E=engine D/A=shift Q=quit");

        ImGui::End();

        // --- Plot window ---
        ImGui::SetNextWindowPos(ImVec2(420, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(850, 700), ImGuiCond_FirstUseEver);
        ImGui::Begin("Engine Telemetry");

        if (ImPlot::BeginPlot("RPM", ImVec2(-1, 200))) {
            ImPlot::SetupAxes("Time", "RPM");
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 10000, ImPlotCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, PlotHistory::kMaxSize, ImPlotCond_Always);
            ImPlot::PlotLine("RPM", rpm_history.data, rpm_history.count);
            ImPlot::EndPlot();
        }

        if (ImPlot::BeginPlot("Throttle & Load", ImVec2(-1, 200))) {
            ImPlot::SetupAxes("Time", "%");
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 110, ImPlotCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, PlotHistory::kMaxSize, ImPlotCond_Always);
            ImPlot::PlotLine("Throttle", throttle_history.data, throttle_history.count);
            ImPlot::PlotLine("Load", load_history.data, load_history.count);
            ImPlot::EndPlot();
        }

        if (ImPlot::BeginPlot("Gear", ImVec2(-1, 150))) {
            ImPlot::SetupAxes("Time", "Gear");
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 8, ImPlotCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, PlotHistory::kMaxSize, ImPlotCond_Always);
            ImPlot::PlotLine("Gear", gear_history.data, gear_history.count);
            ImPlot::EndPlot();
        }

        ImGui::End();

        // --- Render ---
        ImGui::Render();
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    SDL_PauseAudioDevice(audio_dev, 1);
    SDL_CloseAudioDevice(audio_dev);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
