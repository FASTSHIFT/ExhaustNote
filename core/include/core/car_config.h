#pragma once

#include "core/types.h"
#include <cstdint>
#include <string>
#include <vector>

namespace exhaust {

/// Runtime car configuration loaded from JSON.
struct CarConfig {
    std::string name;
    std::string engine_type;
    std::string base_dir; ///< Directory containing WAV files

    // Engine
    uint8_t cylinders = 8;
    float rpm_idle = 900.0f;
    float rpm_redline = 9000.0f;
    float peak_torque = 500.0f;
    float peak_torque_rpm = 6000.0f;
    float inertia = 0.12f;

    // Transmission
    std::vector<float> gear_ratios;
    float final_drive = 4.0f;

    // Audio layers
    struct Layer {
        std::string file;
        float rpm;
    };
    std::vector<Layer> onload;
    std::vector<Layer> offload;
};

/// Load a car configuration from a JSON file.
/// @param json_path  Path to car.json
/// @param config     Output config struct
/// @return true on success
bool load_car_config(const std::string& json_path, CarConfig& config);

/// Scan a directory for car.json files and return list of available cars.
/// @param cars_dir  Root directory containing car subdirectories
/// @return Vector of (display_name, json_path) pairs
std::vector<std::pair<std::string, std::string>> scan_cars(const std::string& cars_dir);

} // namespace exhaust
