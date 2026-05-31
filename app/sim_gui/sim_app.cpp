#include "sim_app.h"
#include "sim_input.h"
#include "sim_physics.h"

#include "core/car_config.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "implot.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <cstdio>

namespace exhaust {

// --- SDL keycode → SimEvent::Key translation ---
static Key translate_sdl_key(SDL_Keycode sym)
{
    switch (sym) {
    case SDLK_w:
    case SDLK_UP:
        return Key::W;
    case SDLK_a:
        return Key::A;
    case SDLK_s:
        return Key::S;
    case SDLK_d:
        return Key::D;
    case SDLK_e:
        return Key::E;
    case SDLK_q:
        return Key::Q;
    case SDLK_LEFT:
        return Key::Left;
    case SDLK_RIGHT:
        return Key::Right;
    case SDLK_ESCAPE:
        return Key::Escape;
    case SDLK_SPACE:
        return Key::Space;
    default:
        return Key::Unknown;
    }
}

SimApp::~SimApp()
{
    shutdown();
}

bool SimApp::init(const Config& cfg)
{
    // --- Scan cars ---
    car_list_ = scan_cars(cfg.cars_dir);
    if (car_list_.empty()) {
        std::fprintf(stderr, "No cars found in %s\n", cfg.cars_dir);
        return false;
    }
    std::printf("Found %zu cars in %s\n", car_list_.size(), cfg.cars_dir);
    for (auto& [name, path] : car_list_)
        std::printf("  - %s\n", name.c_str());

    // --- Audio state setup ---
    init_audio_state();
    generate_combustion_pulse(pulse_sample_, 512);
    load_backfire_sample(std::string(cfg.cars_dir) + "/backfire/backfireEXT_1.wav", audio_state_);

    // --- Load first car ---
    load_car(car_list_[0].second, audio_state_, transmission_, pulse_sample_, 512);

    // --- SDL + OpenGL ---
    if (!init_sdl(cfg))
        return false;

    // --- Audio device ---
    if (!init_audio(cfg))
        return false;

    std::printf("\nReady! Use the GUI or keyboard:\n");
    std::printf("  W/Up = throttle, E = engine on/off\n");
    std::printf("  D/Right = shift up, A/Left = shift down\n");

    return true;
}

bool SimApp::init_sdl(const Config& cfg)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "SDL init error: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    window_ = SDL_CreateWindow(
        cfg.title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        cfg.window_w, cfg.window_h,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    gl_context_ = SDL_GL_CreateContext(window_);
    SDL_GL_MakeCurrent(window_, gl_context_);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_);
    ImGui_ImplOpenGL3_Init("#version 330");

    return true;
}

bool SimApp::init_audio(const Config& cfg)
{
    SDL_AudioSpec desired = {};
    desired.freq = cfg.audio_freq;
    desired.format = AUDIO_S16SYS;
    desired.channels = 1;
    desired.samples = static_cast<Uint16>(cfg.audio_samples);
    desired.callback = audio_callback;
    desired.userdata = &audio_state_;

    SDL_AudioSpec obtained;
    audio_dev_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (audio_dev_ == 0) {
        std::fprintf(stderr, "Audio open failed: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

void SimApp::init_audio_state()
{
    audio_state_.mixer.set_master_volume(master_volume_);

    IdleFluctuation::Config idle_cfg;
    idle_cfg.amplitude = 25.0f;
    idle_cfg.rate = 1.0f;
    idle_cfg.rpm_threshold = 1500.0f;
    audio_state_.idle_fluct.set_config(idle_cfg);

    ThrottleTransient::Config tt_cfg;
    tt_cfg.attack_gain = 1.25f;
    tt_cfg.attack_time = 0.06f;
    tt_cfg.decay_time = 0.15f;
    audio_state_.transient.set_config(tt_cfg);
}

void SimApp::run()
{
    running_ = true;
    Uint32 last_tick = SDL_GetTicks();

    while (running_) {
        // --- Events: translate SDL → SimEvent ---
        SDL_Event sdl_event;
        while (SDL_PollEvent(&sdl_event)) {
            ImGui_ImplSDL2_ProcessEvent(&sdl_event);
            if (sdl_event.type == SDL_QUIT) {
                running_ = false;
                break;
            }

            SimEvent ev;
            if (sdl_event.type == SDL_KEYDOWN) {
                ev.type = SimEvent::Type::KeyDown;
                ev.key = translate_sdl_key(sdl_event.key.keysym.sym);
                ev.repeat = sdl_event.key.repeat != 0;
            } else if (sdl_event.type == SDL_KEYUP) {
                ev.type = SimEvent::Type::KeyUp;
                ev.key = translate_sdl_key(sdl_event.key.keysym.sym);
                ev.repeat = false;
            }

            if (ev.type != SimEvent::Type::None) {
                for (auto& cb : event_cbs_)
                    cb(ev);
            }
        }

        // --- Timing ---
        Uint32 now = SDL_GetTicks();
        float dt = (now - last_tick) / 1000.0f;
        last_tick = now;
        if (dt > 0.1f)
            dt = 0.1f;

        // --- Input ---
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        sim_input_update(state_, keys, dt);

        // --- Engine on/off transitions ---
        if (state_.engine_on != prev_engine_on_) {
            if (state_.engine_on) {
                SDL_PauseAudioDevice(audio_dev_, 0);
                transmission_.reset();
            } else {
                SDL_PauseAudioDevice(audio_dev_, 1);
            }
            prev_engine_on_ = state_.engine_on;
        }

        // --- Physics ---
        sim_physics_update(state_, transmission_, dt);

        // --- Audio sync ---
        {
            std::lock_guard<std::mutex> lock(audio_state_.mutex);
            audio_state_.params.rpm = state_.smoothed_rpm;
            audio_state_.params.throttle = state_.throttle;
            audio_state_.params.load = state_.load;
            audio_state_.throttle = state_.throttle;
            audio_state_.dt = dt;
            audio_state_.afterfire_level = state_.afterfire;
            audio_state_.engine_running = state_.engine_on;
        }
        audio_state_.mixer.set_master_volume(master_volume_);

        // --- User update callbacks ---
        for (auto& cb : update_cbs_)
            cb(dt);

        // --- Car switch handling ---
        if (state_.car_switch_target == -2) {
            transmission_.shift_down();
            state_.car_switch_target = -1;
        } else if (state_.car_switch_target == -3) {
            transmission_.shift_up();
            state_.car_switch_target = -1;
        }
        if (state_.car_switch_requested) {
            switch_car(state_.car_switch_target);
            state_.car_switch_requested = false;
            state_.car_switch_target = -1;
        }

        // --- ImGui frame ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        for (auto& cb : render_cbs_)
            cb();

        // --- GL render ---
        ImGui::Render();
        int w, h;
        SDL_GetWindowSize(window_, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window_);
    }
}

void SimApp::switch_car(int index)
{
    state_.engine_on = false;
    prev_engine_on_ = false;
    SDL_PauseAudioDevice(audio_dev_, 1);
    state_.current_car = index;
    load_car(car_list_[index].second, audio_state_, transmission_, pulse_sample_, 512);
}

void SimApp::shutdown()
{
    if (audio_dev_) {
        SDL_PauseAudioDevice(audio_dev_, 1);
        SDL_CloseAudioDevice(audio_dev_);
        audio_dev_ = 0;
    }

    if (window_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();

        SDL_GL_DeleteContext(gl_context_);
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        gl_context_ = nullptr;

        SDL_Quit();
    }
}

} // namespace exhaust
