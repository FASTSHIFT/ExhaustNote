#include "sim_physics.h"

#include <cmath>

namespace exhaust {

void sim_physics_update(SimState& state, Transmission& trans, float dt)
{
    if (!state.engine_on)
        return;

    // Apply physics config to transmission (single point of truth)
    trans.set_engine_brake(state.physics.engine_brake_nm);
    trans.set_road_load_coeff(state.physics.road_coeff);

    // External load: base load + brake force if braking
    if (state.braking) {
        trans.set_external_load(state.physics.load_nm + state.physics.brake_force_nm);
    } else {
        trans.set_external_load(state.physics.load_nm);
    }

    // Step the transmission physics
    trans.update(state.throttle, dt);

    // RPM low-pass filter (simulates mechanical response lag)
    // Time constant ~80ms: fast enough to follow revving, slow enough to smooth shifts
    float rpm_tc = 0.08f;
    float rpm_alpha = 1.0f - std::exp(-dt / rpm_tc);
    state.smoothed_rpm += rpm_alpha * (trans.rpm() - state.smoothed_rpm);

    // Read back derived state
    state.speed_kmh = trans.speed_kmh();
    state.gear = trans.gear();
    state.load = trans.load();
    state.rev_limiter = trans.rev_limiter_active();
    state.afterfire = trans.afterfire();

    // Push to plot history
    state.rpm_history.push(state.smoothed_rpm);
    state.throttle_history.push(state.throttle * 100.0f);
    state.load_history.push(state.load * 100.0f);
    state.speed_history.push(state.speed_kmh);
}

} // namespace exhaust
