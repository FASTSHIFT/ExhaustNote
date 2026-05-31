#pragma once

#include "sim_audio.h"
#include "sim_event.h"
#include "sim_state.h"

#include "core/transmission.h"

#include <functional>
#include <string>
#include <vector>

struct SDL_Window;
typedef void* SDL_GLContext;
typedef uint32_t SDL_AudioDeviceID;

namespace exhaust {

/// Lightweight callback-based application framework.
/// Encapsulates SDL/GL/Audio init and the main loop.
/// Users register callbacks for custom behavior.
class SimApp {
public:
    /// Callback types
    using EventCallback = std::function<void(const SimEvent& event)>;
    using UpdateCallback = std::function<void(float dt)>;
    using RenderCallback = std::function<void()>;

    struct Config {
        const char* title = "ExhaustNote Simulator";
        const char* cars_dir = "/tmp/ac_cars";
        int window_w = 1280;
        int window_h = 720;
        int audio_freq = 44100;
        int audio_samples = 512;
    };

    SimApp() = default;
    ~SimApp();

    // Non-copyable
    SimApp(const SimApp&) = delete;
    SimApp& operator=(const SimApp&) = delete;

    /// Initialize SDL, OpenGL, ImGui, Audio. Returns false on failure.
    bool init() { return init(Config {}); }
    bool init(const Config& config);

    /// Register a callback for input events (platform-agnostic).
    void on_event(EventCallback cb) { event_cbs_.push_back(std::move(cb)); }

    /// Register a callback for per-frame update (after input, before render).
    void on_update(UpdateCallback cb) { update_cbs_.push_back(std::move(cb)); }

    /// Register a callback for ImGui rendering (between NewFrame and Render).
    void on_render(RenderCallback cb) { render_cbs_.push_back(std::move(cb)); }

    /// Run the main loop. Blocks until quit.
    void run();

    /// Request exit from the main loop.
    void quit() { running_ = false; }

    // --- Accessors for shared state ---
    SimState& state() { return state_; }
    AudioState& audio() { return audio_state_; }
    Transmission& transmission() { return transmission_; }
    const std::vector<std::pair<std::string, std::string>>& cars() const { return car_list_; }
    float& master_volume() { return master_volume_; }
    SDL_AudioDeviceID audio_device() const { return audio_dev_; }

    /// Load a car by index. Handles engine stop + audio pause.
    void switch_car(int index);

private:
    bool init_sdl(const Config& cfg);
    bool init_audio(const Config& cfg);
    void init_audio_state();
    void shutdown();

    // SDL
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    SDL_AudioDeviceID audio_dev_ = 0;

    // State
    SimState state_;
    AudioState audio_state_;
    Transmission transmission_;
    std::vector<std::pair<std::string, std::string>> car_list_;
    float master_volume_ = 0.8f;
    bool running_ = false;
    bool prev_engine_on_ = false;

    // Pulse sample (static lifetime)
    sample_t pulse_sample_[512] = {};

    // Callbacks
    std::vector<EventCallback> event_cbs_;
    std::vector<UpdateCallback> update_cbs_;
    std::vector<RenderCallback> render_cbs_;
};

} // namespace exhaust
