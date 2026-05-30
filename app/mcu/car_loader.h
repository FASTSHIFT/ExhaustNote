/**
 * Car Loader for MCU
 *
 * Responsibilities:
 * - Parse car.json from SD card
 * - Load WAV samples into SDRAM
 * - Configure EngineVoice with loaded layers
 *
 * Memory layout in SDRAM (8MB at 0xC0000000):
 * - WAV samples are allocated sequentially from SDRAM base
 */
#pragma once

#include "core/engine_effects.h"
#include "core/engine_voice.h"
#include "core/transmission.h"

namespace exhaust {

/// SDRAM memory pool for WAV samples
struct SdramPool {
    int16_t* base;     ///< SDRAM base address
    uint32_t capacity; ///< Total capacity in samples (int16)
    uint32_t used;     ///< Samples allocated so far

    /// Allocate n samples from pool. Returns nullptr if full.
    int16_t* alloc(uint32_t n)
    {
        if (used + n > capacity)
            return nullptr;
        int16_t* ptr = base + used;
        used += n;
        return ptr;
    }

    void reset() { used = 0; }
};

/// Loaded car state — everything needed to run the engine.
struct LoadedCar {
    // Engine voice layers
    EngineVoice::LayerConfig onload_layers[kMaxLayers];
    EngineVoice::LayerConfig offload_layers[kMaxLayers];
    uint8_t num_onload = 0;
    uint8_t num_offload = 0;

    // Engine parameters
    uint8_t cylinders = 8;
    float rpm_idle = 900.0f;
    float rpm_redline = 9000.0f;

    // Transmission
    float gear_ratios[8] = {};
    uint8_t num_gears = 0;
    float final_drive = 4.0f;

    // Physics
    float inertia = 0.12f;
    float peak_torque = 500.0f;
    float peak_torque_rpm = 6000.0f;
};

/// Load a car from SD card into SDRAM.
/// @param car_dir   FatFS directory path (e.g. "0:/ExhaustNote/ferrari_458")
/// @param pool      SDRAM memory pool for WAV data
/// @param car       [out] Loaded car configuration
/// @return true on success
bool car_load(const char* car_dir, SdramPool& pool, LoadedCar& car);

/// Apply loaded car config to engine voice and transmission.
void car_apply(const LoadedCar& car, EngineVoice& engine, Transmission& trans,
               CombustionPulse& combustion, IdleFluctuation& idle);

} // namespace exhaust
