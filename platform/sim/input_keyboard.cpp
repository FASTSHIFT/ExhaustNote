#include "platform/input.h"

#include <SDL2/SDL.h>
#include <memory>

namespace exhaust {

class KeyboardInput : public IInput {
public:
    bool init() override
    {
        // SDL event subsystem is initialized with SDL_Init in audio or main
        return true;
    }

    InputState poll() override
    {
        InputState state = {};

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                state.quit = true;
            }
            if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                switch (event.key.keysym.sym) {
                case SDLK_e:
                    state.engine_toggle = true;
                    break;
                case SDLK_d:
                case SDLK_RIGHT:
                    state.shift_up = true;
                    break;
                case SDLK_a:
                case SDLK_LEFT:
                    state.shift_down = true;
                    break;
                case SDLK_ESCAPE:
                case SDLK_q:
                    state.quit = true;
                    break;
                default:
                    break;
                }
            }
        }

        // Continuous throttle from held keys
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) {
            throttle_ += 0.02f;
            if (throttle_ > 1.0f)
                throttle_ = 1.0f;
        } else {
            throttle_ -= 0.01f;
            if (throttle_ < 0.0f)
                throttle_ = 0.0f;
        }

        state.throttle = throttle_;
        return state;
    }

private:
    float throttle_ = 0.0f;
};

/// Factory function to create keyboard input.
std::unique_ptr<IInput> create_input()
{
    return std::make_unique<KeyboardInput>();
}

} // namespace exhaust
