#include "gui_gauges.h"
#include "sim_audio.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "implot.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace exhaust;

// --- Configuration ---
static const char* CARS_DIR = "/tmp/ac_cars";

// --- History buffer for plotting ---
struct PlotHistory {
    static constexpr int kMaxSize = 600;
    float data[kMaxSize] = {};
    int count = 0;
    void push(float val)
    {
        if (count < kMaxSize)
            data[count++] = val;
        else {
            std::memmove(data, data + 1, (kMaxSize - 1) * sizeof(float));
            data[kMaxSize - 1] = val;
        }
    }
};

int main(int, char**)
{
    // --- Scan available cars ---
    auto car_list = scan_cars(CARS_DIR);
    if (car_list.empty()) {
        std::fprintf(stderr, "No cars found in %s\n", CARS_DIR);
        return 1;
    }
    std::printf("Found %zu cars in %s\n", car_list.size(), CARS_DIR);
    for (auto& [name, path] : car_list)
        std::printf("  - %s\n", name.c_str());

    // --- Setup audio ---
    static sample_t pulse_sample[512];
    generate_combustion_pulse(pulse_sample, 512);

    AudioState audio_state;
    audio_state.mixer.set_master_volume(0.8f);

    IdleFluctuation::Config idle_cfg;
    idle_cfg.amplitude = 25.0f;
    idle_cfg.rate = 1.0f;
    idle_cfg.rpm_threshold = 1500.0f;
    audio_state.idle_fluct.set_config(idle_cfg);

    ThrottleTransient::Config tt_cfg;
    tt_cfg.attack_gain = 1.25f;
    tt_cfg.attack_time = 0.06f;
    tt_cfg.decay_time = 0.15f;
    audio_state.transient.set_config(tt_cfg);

    load_backfire_sample("/tmp/ac_cars/backfire/backfireEXT_1.wav", audio_state);

    // --- Load first car ---
    Transmission transmission;
    int current_car = 0;
    load_car(car_list[0].second, audio_state, transmission, pulse_sample, 512);

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
        "ExhaustNote Simulator",
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
    float smoothed_rpm = 900.0f; // Low-pass filtered RPM for audio/display
    PlotHistory rpm_history, throttle_history, load_history, speed_history;

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

        // RPM low-pass filter (simulates mechanical response lag)
        // Time constant ~80ms: fast enough to follow revving, slow enough to smooth shifts
        {
            float rpm_tc = 0.08f; // 80ms time constant
            float rpm_alpha = 1.0f - std::exp(-dt / rpm_tc);
            smoothed_rpm += rpm_alpha * (transmission.rpm() - smoothed_rpm);
        }

        // Update audio params (use smoothed RPM for natural sound transitions)
        {
            std::lock_guard<std::mutex> lock(audio_state.mutex);
            audio_state.params.rpm = smoothed_rpm;
            audio_state.params.throttle = throttle;
            audio_state.params.load = transmission.load();
            audio_state.throttle = throttle;
            audio_state.dt = dt;
            audio_state.afterfire_level = transmission.afterfire();
            audio_state.engine_running = engine_on;
        }

        // Push to history (use smoothed RPM for display too)
        rpm_history.push(smoothed_rpm);
        throttle_history.push(throttle * 100.0f);
        load_history.push(transmission.load() * 100.0f);
        speed_history.push(transmission.speed_kmh());

        // --- ImGui Frame ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Main window
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 720), ImGuiCond_FirstUseEver);
        ImGui::Begin("ExhaustNote - Controls");

        // Car selector
        ImGui::Text("Vehicle");
        if (ImGui::BeginCombo("##car", car_list[current_car].first.c_str())) {
            for (int i = 0; i < static_cast<int>(car_list.size()); ++i) {
                bool selected = (i == current_car);
                if (ImGui::Selectable(car_list[i].first.c_str(), selected)) {
                    if (i != current_car) {
                        current_car = i;
                        engine_on = false;
                        SDL_PauseAudioDevice(audio_dev, 1);
                        load_car(car_list[i].second, audio_state,
                            transmission, pulse_sample, 512);
                    }
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::Text("%s | %d cyl | Redline: %.0f",
            current_car_config().engine_type.c_str(), current_car_config().cylinders, current_car_config().rpm_redline);
        ImGui::Separator();

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

        // RPM bar (uses smoothed RPM)
        ImGui::Text("RPM");
        float rpm_frac = (smoothed_rpm - current_car_config().rpm_idle) / (current_car_config().rpm_redline - current_car_config().rpm_idle);
        rpm_frac = std::fmax(0.0f, std::fmin(1.0f, rpm_frac));
        ImVec4 rpm_color = transmission.rev_limiter_active()
            ? ImVec4(1, 0.2f, 0.2f, 1)
            : (rpm_frac > 0.85f ? ImVec4(1, 0.6f, 0, 1) : ImVec4(0.2f, 0.8f, 0.2f, 1));
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, rpm_color);
        ImGui::ProgressBar(rpm_frac, ImVec2(-1, 24), "");
        ImGui::PopStyleColor();
        ImGui::Text("%.0f / %.0f RPM %s", smoothed_rpm, current_car_config().rpm_redline,
            transmission.rev_limiter_active() ? "[LIMITER]" : "");

        // Speed bar
        float speed = transmission.speed_kmh();
        float speed_frac = std::fmin(speed / 350.0f, 1.0f);
        ImGui::Text("Speed");
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.3f, 0.5f, 1.0f, 1.0f));
        ImGui::ProgressBar(speed_frac, ImVec2(-1, 20), "");
        ImGui::PopStyleColor();
        ImGui::Text("%.0f km/h", speed);

        // Fixed-height status line (no layout bounce)
        if (transmission.afterfire() > 0.01f) {
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "BACKFIRE %.0f%%",
                transmission.afterfire() * 100.0f);
        } else {
            ImGui::TextColored(ImVec4(0, 0, 0, 0), "."); // Invisible placeholder
        }

        ImGui::Spacing();

        // Gear display
        ImGui::PushFont(nullptr); // Use default font but larger
        char gear_str[8];
        std::snprintf(gear_str, sizeof(gear_str), "%d", transmission.gear());
        ImGui::Text("Gear:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s / %d",
            gear_str, (int)current_car_config().gear_ratios.size());
        ImGui::SameLine(250);
        if (ImGui::Button("Down [A]"))
            transmission.shift_down();
        ImGui::SameLine();
        if (ImGui::Button("Up [D]"))
            transmission.shift_up();
        ImGui::PopFont();

        ImGui::Spacing();

        // Throttle slider (directly controls engine torque via physics model)
        ImGui::Text("Throttle [W/S or drag slider]");
        float throttle_pct = throttle * 100.0f;
        if (ImGui::SliderFloat("##throttle", &throttle_pct, 0.0f, 100.0f, "%.0f%%")) {
            throttle = throttle_pct / 100.0f;
        }

        ImGui::Spacing();

        // External load slider (simulates drivetrain resistance)
        // Load scales quadratically with RPM (like real aero + road load)
        ImGui::Text("Load (road resistance)");
        static float load_nm = 200.0f; // Default: normal driving
        ImGui::SliderFloat("##load", &load_nm, 0.0f, 500.0f, "%.0f Nm");
        transmission.set_external_load(load_nm);
        ImGui::Text("0=neutral, 200=road, 400=hill, 500=max");

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

        if (ImPlot::BeginPlot("Speed", ImVec2(-1, 150))) {
            ImPlot::SetupAxes("Time", "km/h");
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 350, ImPlotCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, PlotHistory::kMaxSize, ImPlotCond_Always);
            ImPlot::PlotLine("Speed", speed_history.data, speed_history.count);
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
