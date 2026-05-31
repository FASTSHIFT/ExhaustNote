#include "sim_app.h"
#include "sim_event.h"
#include "sim_gui.h"

using namespace exhaust;

int main(int, char**)
{
    SimApp app;

    if (!app.init())
        return 1;

    // --- Event: keyboard shortcuts (no SDL dependency) ---
    app.on_event([&](const SimEvent& event) {
        if (event.type != SimEvent::Type::KeyDown || event.repeat)
            return;

        switch (event.key) {
        case Key::E:
            app.state().engine_on = !app.state().engine_on;
            break;
        case Key::D:
        case Key::Right:
            app.transmission().shift_up();
            break;
        case Key::A:
        case Key::Left:
            app.transmission().shift_down();
            break;
        case Key::Escape:
        case Key::Q:
            app.quit();
            break;
        default:
            break;
        }
    });

    // --- Render: GUI windows ---
    app.on_render([&]() {
        sim_gui_controls(app.state(), app.cars(), app.master_volume());
        sim_gui_telemetry(app.state());
    });

    app.run();
    return 0;
}
