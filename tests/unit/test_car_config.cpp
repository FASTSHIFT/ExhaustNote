#include "core/car_config.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <unistd.h>

#include <gtest/gtest.h>

using namespace exhaust;

// Helper: write a temp JSON file
static std::string write_temp_json(const std::string& content)
{
    std::string path = "/tmp/test_car_config.json";
    std::ofstream f(path);
    f << content;
    f.close();
    return path;
}

TEST(CarConfigTest, LoadValidConfig)
{
    std::string json = R"({
        "name": "Test Car",
        "engine": "V8 4.0L",
        "cylinders": 8,
        "rpm_idle": 900,
        "rpm_redline": 8000,
        "peak_torque": 500,
        "peak_torque_rpm": 5000,
        "inertia": 0.12,
        "transmission": {
            "gears": [3.0, 2.0, 1.5, 1.0],
            "final_drive": 4.0
        },
        "onload": [
            {"file": "idle.wav", "rpm": 900},
            {"file": "on3000.wav", "rpm": 3000}
        ],
        "offload": [
            {"file": "idle.wav", "rpm": 900},
            {"file": "off3000.wav", "rpm": 3000}
        ]
    })";

    std::string path = write_temp_json(json);
    CarConfig cfg;
    ASSERT_TRUE(load_car_config(path, cfg));

    EXPECT_EQ(cfg.name, "Test Car");
    EXPECT_EQ(cfg.engine_type, "V8 4.0L");
    EXPECT_EQ(cfg.cylinders, 8);
    EXPECT_FLOAT_EQ(cfg.rpm_idle, 900.0f);
    EXPECT_FLOAT_EQ(cfg.rpm_redline, 8000.0f);
    EXPECT_FLOAT_EQ(cfg.peak_torque, 500.0f);
    EXPECT_FLOAT_EQ(cfg.peak_torque_rpm, 5000.0f);
    EXPECT_FLOAT_EQ(cfg.inertia, 0.12f);
    EXPECT_FLOAT_EQ(cfg.final_drive, 4.0f);

    ASSERT_EQ(cfg.gear_ratios.size(), 4u);
    EXPECT_FLOAT_EQ(cfg.gear_ratios[0], 3.0f);
    EXPECT_FLOAT_EQ(cfg.gear_ratios[3], 1.0f);

    ASSERT_EQ(cfg.onload.size(), 2u);
    EXPECT_EQ(cfg.onload[0].file, "idle.wav");
    EXPECT_FLOAT_EQ(cfg.onload[0].rpm, 900.0f);

    ASSERT_EQ(cfg.offload.size(), 2u);
    EXPECT_EQ(cfg.offload[1].file, "off3000.wav");
}

TEST(CarConfigTest, LoadInvalidPath)
{
    CarConfig cfg;
    EXPECT_FALSE(load_car_config("/nonexistent/path.json", cfg));
}

TEST(CarConfigTest, LoadInvalidJson)
{
    std::string path = write_temp_json("{ invalid json }}}");
    CarConfig cfg;
    EXPECT_FALSE(load_car_config(path, cfg));
}

TEST(CarConfigTest, ScanCarsDirectory)
{
    // Create a temporary car directory for testing
    std::string tmp_dir = "/tmp/exhaust_test_cars_" + std::to_string(getpid());
    std::string car_dir = tmp_dir + "/test_car";
    system(("mkdir -p " + car_dir).c_str());
    // Write a minimal car.json
    FILE* f = fopen((car_dir + "/car.json").c_str(), "w");
    fprintf(f, "{\"name\":\"Test\",\"cylinders\":4,\"onload\":[]}");
    fclose(f);

    auto cars = scan_cars(tmp_dir);
    EXPECT_GE(cars.size(), 1u);

    // Cleanup
    system(("rm -rf " + tmp_dir).c_str());

    // Each entry should have a non-empty name and valid path
    for (auto& [name, path] : cars) {
        EXPECT_FALSE(name.empty());
        EXPECT_FALSE(path.empty());
    }
}

TEST(CarConfigTest, ScanEmptyDirectory)
{
    auto cars = scan_cars("/tmp/nonexistent_dir_12345");
    EXPECT_TRUE(cars.empty());
}
