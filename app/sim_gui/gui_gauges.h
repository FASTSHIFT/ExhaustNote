#pragma once

#include "imgui.h"

/// Draw a needle-style arc gauge.
/// @param label     Text to display below the gauge
/// @param value     Current value (0-1 normalized)
/// @param width     Available width
/// @param height    Gauge height
/// @param arc_color Color of the arc background
/// @param red_zone  Fraction at which red zone starts (0.85 = last 15%)
/// @param is_alert  If true, needle turns red
void draw_arc_gauge(const char* label, float value, float width, float height,
    ImU32 arc_color, float red_zone = 0.85f, bool is_alert = false);

/// Draw RPM gauge with green/red zones.
void draw_rpm_gauge(float rpm, float rpm_idle, float rpm_redline, float width,
    bool limiter_active);

/// Draw speed gauge.
void draw_speed_gauge(float speed_kmh, float max_speed, float width);
