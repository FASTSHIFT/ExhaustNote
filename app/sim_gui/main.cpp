#include "sim_audio.h"
#include "sim_gui.h"
#include "sim_input.h"
#include "sim_physics.h"
#include "sim_state.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "implot.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <cstdio>
#include <mutex>

using namespace exhaust;

static const char* CARS_DIR = "/tmp/ac_cars";

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
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    // --- Init Audio Device ---
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

    // --- Sim State ---
    SimState state;
    float master_volume = 0.8f;

    std::printf("\nReady! Use the GUI or keyboard:\n");
    std::printf("  W/Up = throttle, E = engine on/off\n");
    std::printf("  D/Right = shift up, A/Left = shift down\n");

    // --- Main loop ---
    bool running = true;
    Uint32 last_tick = SDL_GetTicks();
    bool prev_engine_on = false;

    while (running) {
        // --- Event handling (discrete keys) ---
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                running = false;
            if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                switch (event.key.keysym.sym) {
                case SDLK_e:
                    state.engine_on = !state.engine_on;
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

        // --- Timing ---
        Uint32 now = SDL_GetTicks();
        float dt = (now - last_tick) / 1000.0f;
        last_tick = now;
        if (dt > 0.1f)
            dt = 0.1f;

        // --- Input → State ---
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        sim_input_update(state, keys, dt);

        // --- Handle engine on/off transitions ---
        if (state.engine_on != prev_engine_on) {
            if (state.engine_on) {
                SDL_PauseAudioDevice(audio_dev, 0);
                transmission.reset();
            } else {
                SDL_PauseAudioDevice(audio_dev, 1);
            }
            prev_engine_on = state.engine_on;
        }

        // --- Physics → State ---
        sim_physics_update(state, transmission, dt);

        // --- Audio sync ---
        {
            std::lock_guard<std::mutex> lock(audio_state.mutex);
            audio_state.params.rpm = state.smoothed_rpm;
            audio_state.params.throttle = state.throttle;
            audio_state.params.load = state.load;
            audio_state.throttle = state.throttle;
            audio_state.dt = dt;
            audio_state.afterfire_level = state.afterfire;
            audio_state.engine_running = state.engine_on;
        }
        audio_state.mixer.set_master_volume(master_volume);

        // --- GUI ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        sim_gui_controls(state, car_list, master_volume);
        sim_gui_telemetry(state);

        // --- Handle GUI shift buttons (encoded as special car_switch_target values) ---
        if (state.car_switch_target == -2) {
            transmission.shift_down();
            state.car_switch_target = -1;
        } else if (state.car_switch_target == -3) {
            transmission.shift_up();
            state.car_switch_target = -1;
        }

        // --- Handle car switch ---
        if (state.car_switch_requested) {
            state.engine_on = false;
            prev_engine_on = false;
            SDL_PauseAudioDevice(audio_dev, 1);
            state.current_car = state.car_switch_target;
            load_car(car_list[state.current_car].second, audio_state,
                transmission, pulse_sample, 512);
            state.car_switch_requested = false;
            state.car_switch_target = -1;
        }

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

    // --- Cleanup ---
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
