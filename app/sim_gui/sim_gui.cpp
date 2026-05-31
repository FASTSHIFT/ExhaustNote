#include "sim_gui.h"
#include "sim_audio.h"

#include "imgui.h"
#include "implot.h"

#include <cmath>
#include <cstdio>

namespace exhaust {

void sim_gui_controls(SimState& state,
    const std::vector<std::pair<std::string, std::string>>& car_list,
    float& master_volume)
{
    const auto& cfg = current_car_config();

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(400, 720), ImGuiCond_Always);
    ImGui::Begin("ExhaustNote - Controls", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    // --- Car selector ---
    ImGui::Text("Vehicle");
    if (ImGui::BeginCombo("##car", car_list[state.current_car].first.c_str())) {
        for (int i = 0; i < static_cast<int>(car_list.size()); ++i) {
            bool selected = (i == state.current_car);
            if (ImGui::Selectable(car_list[i].first.c_str(), selected)) {
                if (i != state.current_car) {
                    state.car_switch_requested = true;
                    state.car_switch_target = i;
                }
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::Text("%s | %d cyl | Redline: %.0f",
        cfg.engine_type.c_str(), cfg.cylinders, cfg.rpm_redline);
    ImGui::Separator();

    // --- Engine state ---
    ImVec4 color = state.engine_on
        ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f)
        : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
    ImGui::TextColored(color, "Engine: %s", state.engine_on ? "RUNNING" : "OFF");
    ImGui::SameLine();
    if (ImGui::Button(state.engine_on ? "Stop [E]" : "Start [E]")) {
        state.engine_on = !state.engine_on;
    }

    ImGui::Separator();

    // --- RPM bar ---
    ImGui::Text("RPM");
    float rpm_frac = (state.smoothed_rpm - cfg.rpm_idle)
        / (cfg.rpm_redline - cfg.rpm_idle);
    rpm_frac = std::fmax(0.0f, std::fmin(1.0f, rpm_frac));
    ImVec4 rpm_color = state.rev_limiter
        ? ImVec4(1, 0.2f, 0.2f, 1)
        : (rpm_frac > 0.85f ? ImVec4(1, 0.6f, 0, 1) : ImVec4(0.2f, 0.8f, 0.2f, 1));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, rpm_color);
    ImGui::ProgressBar(rpm_frac, ImVec2(-1, 24), "");
    ImGui::PopStyleColor();
    ImGui::Text("%.0f / %.0f RPM %s", state.smoothed_rpm, cfg.rpm_redline,
        state.rev_limiter ? "[LIMITER]" : "");

    // --- Speed bar ---
    float speed_frac = std::fmin(state.speed_kmh / 350.0f, 1.0f);
    ImGui::Text("Speed");
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.3f, 0.5f, 1.0f, 1.0f));
    ImGui::ProgressBar(speed_frac, ImVec2(-1, 20), "");
    ImGui::PopStyleColor();
    ImGui::Text("%.0f km/h", state.speed_kmh);

    // --- Backfire indicator (fixed height to prevent layout bounce) ---
    if (state.afterfire > 0.01f) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "BACKFIRE %.0f%%",
            state.afterfire * 100.0f);
    } else {
        ImGui::TextColored(ImVec4(0, 0, 0, 0), "."); // Invisible placeholder
    }

    ImGui::Spacing();

    // --- Gear display ---
    ImGui::PushFont(nullptr);
    char gear_str[8];
    std::snprintf(gear_str, sizeof(gear_str), "%d", state.gear);
    ImGui::Text("Gear:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s / %d",
        gear_str, static_cast<int>(cfg.gear_ratios.size()));
    ImGui::SameLine(250);
    if (ImGui::Button("Down [A]"))
        state.car_switch_target = -2; // Signal shift down (handled in main)
    ImGui::SameLine();
    if (ImGui::Button("Up [D]"))
        state.car_switch_target = -3; // Signal shift up (handled in main)
    ImGui::PopFont();

    ImGui::Spacing();

    // --- Throttle slider ---
    ImGui::Text("Throttle [W/S or drag slider]");
    float throttle_pct = state.throttle * 100.0f;
    if (ImGui::SliderFloat("##throttle", &throttle_pct, 0.0f, 100.0f, "%.0f%%")) {
        state.throttle = throttle_pct / 100.0f;
    }

    ImGui::Spacing();

    // --- Physics tuning ---
    ImGui::Text("Physics Tuning");
    ImGui::SliderFloat("Load (Nm)", &state.physics.load_nm, 0.0f, 500.0f, "%.0f");
    ImGui::SliderFloat("Engine Brake", &state.physics.engine_brake_nm, 10.0f, 200.0f, "%.0f Nm");
    ImGui::SliderFloat("Road Drag", &state.physics.road_coeff, 0.05f, 1.0f, "%.2f");
    if (state.braking)
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "BRAKE [S] (+%.0fNm)", state.physics.brake_force_nm);
    else
        ImGui::Text("S=brake, W=gas");

    ImGui::Spacing();

    // --- Volume ---
    float vol_pct = master_volume * 100.0f;
    if (ImGui::SliderFloat("Volume", &vol_pct, 0.0f, 100.0f, "%.0f%%")) {
        master_volume = vol_pct / 100.0f;
    }

    ImGui::Separator();
    ImGui::Text("Keyboard: W=gas S=brake E=engine D/A=shift Q=quit");

    ImGui::End();
}

void sim_gui_telemetry(const SimState& state)
{
    ImGui::SetNextWindowPos(ImVec2(420, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(850, 700), ImGuiCond_Always);
    ImGui::Begin("Engine Telemetry", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    if (ImPlot::BeginPlot("RPM", ImVec2(-1, 200))) {
        ImPlot::SetupAxes("Time", "RPM");
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 10000, ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, PlotHistory::kMaxSize, ImPlotCond_Always);
        ImPlot::PlotLine("RPM", state.rpm_history.data, state.rpm_history.count);
        ImPlot::EndPlot();
    }

    if (ImPlot::BeginPlot("Throttle & Load", ImVec2(-1, 200))) {
        ImPlot::SetupAxes("Time", "%");
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 110, ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, PlotHistory::kMaxSize, ImPlotCond_Always);
        ImPlot::PlotLine("Throttle", state.throttle_history.data, state.throttle_history.count);
        ImPlot::PlotLine("Load", state.load_history.data, state.load_history.count);
        ImPlot::EndPlot();
    }

    if (ImPlot::BeginPlot("Speed", ImVec2(-1, 150))) {
        ImPlot::SetupAxes("Time", "km/h");
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 350, ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, PlotHistory::kMaxSize, ImPlotCond_Always);
        ImPlot::PlotLine("Speed", state.speed_history.data, state.speed_history.count);
        ImPlot::EndPlot();
    }

    ImGui::End();
}

} // namespace exhaust
