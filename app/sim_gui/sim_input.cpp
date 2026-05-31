#include "sim_input.h"

namespace exhaust {

void sim_input_update(SimState& state, const Uint8* keys, float dt)
{
    // Brake (continuous hold)
    state.braking = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];

    // Throttle ramp up / decay
    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) {
        state.throttle += 3.0f * dt;
        if (state.throttle > 1.0f)
            state.throttle = 1.0f;
    } else {
        state.throttle -= 2.0f * dt;
        if (state.throttle < 0.0f)
            state.throttle = 0.0f;
    }
}

} // namespace exhaust
