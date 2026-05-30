#include "core/car_config.h"

#include <cJSON.h>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

namespace exhaust {

static std::string read_file(const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
        return "";
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::string content(static_cast<size_t>(size), '\0');
    std::fread(&content[0], 1, static_cast<size_t>(size), f);
    std::fclose(f);
    return content;
}

static std::string dir_of(const std::string& path)
{
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos)
        return ".";
    return path.substr(0, pos);
}

bool load_car_config(const std::string& json_path, CarConfig& config)
{
    std::string content = read_file(json_path);
    if (content.empty()) {
        std::fprintf(stderr, "Cannot read: %s\n", json_path.c_str());
        return false;
    }

    cJSON* root = cJSON_Parse(content.c_str());
    if (!root) {
        std::fprintf(stderr, "JSON parse error in: %s\n", json_path.c_str());
        return false;
    }

    config.base_dir = dir_of(json_path);

    // Basic info
    cJSON* item;
    if ((item = cJSON_GetObjectItem(root, "name")))
        config.name = item->valuestring;
    if ((item = cJSON_GetObjectItem(root, "engine")))
        config.engine_type = item->valuestring;

    // Engine params
    if ((item = cJSON_GetObjectItem(root, "cylinders")))
        config.cylinders = static_cast<uint8_t>(item->valueint);
    if ((item = cJSON_GetObjectItem(root, "rpm_idle")))
        config.rpm_idle = static_cast<float>(item->valuedouble);
    if ((item = cJSON_GetObjectItem(root, "rpm_redline")))
        config.rpm_redline = static_cast<float>(item->valuedouble);
    if ((item = cJSON_GetObjectItem(root, "peak_torque")))
        config.peak_torque = static_cast<float>(item->valuedouble);
    if ((item = cJSON_GetObjectItem(root, "peak_torque_rpm")))
        config.peak_torque_rpm = static_cast<float>(item->valuedouble);
    if ((item = cJSON_GetObjectItem(root, "inertia")))
        config.inertia = static_cast<float>(item->valuedouble);

    // Transmission
    cJSON* trans = cJSON_GetObjectItem(root, "transmission");
    if (trans) {
        if ((item = cJSON_GetObjectItem(trans, "final_drive")))
            config.final_drive = static_cast<float>(item->valuedouble);
        cJSON* gears = cJSON_GetObjectItem(trans, "gears");
        if (gears && cJSON_IsArray(gears)) {
            config.gear_ratios.clear();
            cJSON* g;
            cJSON_ArrayForEach(g, gears)
            {
                config.gear_ratios.push_back(static_cast<float>(g->valuedouble));
            }
        }
    }

    // Audio layers
    auto parse_layers = [](cJSON* arr, std::vector<CarConfig::Layer>& layers) {
        layers.clear();
        if (!arr || !cJSON_IsArray(arr))
            return;
        cJSON* entry;
        cJSON_ArrayForEach(entry, arr)
        {
            CarConfig::Layer layer;
            cJSON* f = cJSON_GetObjectItem(entry, "file");
            cJSON* r = cJSON_GetObjectItem(entry, "rpm");
            if (f && r) {
                layer.file = f->valuestring;
                layer.rpm = static_cast<float>(r->valuedouble);
                layers.push_back(layer);
            }
        }
    };

    parse_layers(cJSON_GetObjectItem(root, "onload"), config.onload);
    parse_layers(cJSON_GetObjectItem(root, "offload"), config.offload);

    cJSON_Delete(root);
    return true;
}

std::vector<std::pair<std::string, std::string>> scan_cars(const std::string& cars_dir)
{
    std::vector<std::pair<std::string, std::string>> result;

    DIR* dir = opendir(cars_dir.c_str());
    if (!dir)
        return result;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.')
            continue;

        std::string subdir = cars_dir + "/" + entry->d_name;
        struct stat st;
        if (stat(subdir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
            continue;

        std::string json_path = subdir + "/car.json";
        if (stat(json_path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            // Quick-parse just the name
            CarConfig cfg;
            if (load_car_config(json_path, cfg)) {
                std::string display = cfg.name + " (" + cfg.engine_type + ")";
                result.emplace_back(display, json_path);
            }
        }
    }

    closedir(dir);
    return result;
}

} // namespace exhaust
