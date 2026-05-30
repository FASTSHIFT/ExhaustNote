#include "gui_gauges.h"

#include <cmath>
#include <cstdio>

static constexpr float PI = 3.14159265f;

void draw_arc_gauge(const char* label, float value, float width, float height,
    ImU32 arc_color, float red_zone, bool is_alert)
{
    value = std::fmax(0.0f, std::fmin(1.0f, value));

    ImVec2 gauge_pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 center(gauge_pos.x + width * 0.5f, gauge_pos.y + height * 0.85f);
    float radius = height * 0.75f;

    // Arc from 7 o'clock to 5 o'clock (clockwise over top in Y-down coords)
    float arc_start = -PI * 5.0f / 6.0f;
    float arc_end = -PI * 1.0f / 6.0f;
    float arc_span = arc_end - arc_start;

    // Background arc
    if (red_zone < 1.0f) {
        float green_end = arc_start + arc_span * red_zone;
        dl->PathArcTo(center, radius, arc_start, green_end, 25);
        dl->PathStroke(arc_color, 0, 7.0f);
        dl->PathArcTo(center, radius, green_end, arc_end, 10);
        dl->PathStroke(IM_COL32(255, 50, 50, 120), 0, 7.0f);
    } else {
        dl->PathArcTo(center, radius, arc_start, arc_end, 30);
        dl->PathStroke(arc_color, 0, 7.0f);
    }

    // Needle
    float needle_angle = arc_start + arc_span * value;
    ImVec2 needle_end(
        center.x + std::cos(needle_angle) * radius * 0.82f,
        center.y + std::sin(needle_angle) * radius * 0.82f);
    ImU32 needle_col = is_alert ? IM_COL32(255, 50, 50, 255) : IM_COL32(255, 255, 255, 240);
    dl->AddLine(center, needle_end, needle_col, 2.5f);
    dl->AddCircleFilled(center, 4.0f, needle_col);

    // Label text
    ImVec2 text_size = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(center.x - text_size.x * 0.5f, center.y + 6),
        IM_COL32(220, 220, 220, 255), label);

    ImGui::Dummy(ImVec2(width, height + 2));
}

void draw_rpm_gauge(float rpm, float rpm_idle, float rpm_redline, float width,
    bool limiter_active)
{
    float frac = (rpm - rpm_idle) / (rpm_redline - rpm_idle);
    char label[32];
    std::snprintf(label, sizeof(label), "%.0f RPM", rpm);
    draw_arc_gauge(label, frac, width, 80.0f,
        IM_COL32(50, 200, 50, 80), 0.85f, limiter_active);
}

void draw_speed_gauge(float speed_kmh, float max_speed, float width)
{
    float frac = speed_kmh / max_speed;
    char label[32];
    std::snprintf(label, sizeof(label), "%.0f km/h", speed_kmh);
    draw_arc_gauge(label, frac, width, 60.0f,
        IM_COL32(100, 150, 255, 80), 1.0f, false);
}
