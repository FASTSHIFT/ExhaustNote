/**
 * ExhaustNote MCU Firmware - AT32F437 Entry Point
 *
 * Phase 1: Sine wave test through WM8988 CODEC
 * - Initializes system clock (288MHz)
 * - Configures WM8988 via I2C (through PCA9555)
 * - Sets up I2S DMA double-buffering
 * - Outputs 1kHz sine wave to verify audio path
 */

#include "at32f435_437.h"

// BSP includes
extern "C" {
#include "at_surf_f437_board_audio.h"
#include "at_surf_f437_board_delay.h"
#include "at_surf_f437_board_key.h"
#include "at_surf_f437_board_pca9555.h"
#include "at_surf_f437_board_sdram.h"
#include "at_surf_f437_board_variable_resistor.h"
}

// Core engine includes
#include "core/engine_effects.h"
#include "core/engine_voice.h"
#include "core/mixer.h"
#include "core/transmission.h"
#include "core/types.h"

#include <cmath>
#include <cstring>

using namespace exhaust;

// ============================================================
// Audio DMA double buffer
// ============================================================
#define AUDIO_BUFFER_SIZE 512 // samples per half-buffer
#define AUDIO_SAMPLE_RATE 44100

static int16_t dma_buffer[AUDIO_BUFFER_SIZE * 2]; // Double buffer (L+R interleaved would be *4, but mono for now)
static volatile bool buffer_half_ready = false;
static volatile bool buffer_full_ready = false;

// ============================================================
// Engine state (global for ISR access)
// ============================================================
static EngineVoice g_engine;
static Mixer g_mixer;
static CombustionPulse g_combustion;
static IdleFluctuation g_idle_fluct;
static Transmission g_transmission;
static EngineVoice::Params g_params;
static bool g_engine_running = false;

// Combustion pulse sample (generated at startup)
static sample_t g_pulse_sample[512];

// ============================================================
// Phase 1: Simple sine wave generator (for testing)
// ============================================================
static float sine_phase = 0.0f;
static const float SINE_FREQ = 1000.0f; // 1kHz test tone

static void generate_sine(int16_t* buffer, size_t frames)
{
    float phase_inc = SINE_FREQ / static_cast<float>(AUDIO_SAMPLE_RATE);
    for (size_t i = 0; i < frames; ++i) {
        float sample = std::sin(sine_phase * 6.2831853f) * 16000.0f;
        buffer[i] = static_cast<int16_t>(sample);
        sine_phase += phase_inc;
        if (sine_phase >= 1.0f)
            sine_phase -= 1.0f;
    }
}

// ============================================================
// Audio processing (called from DMA ISR)
// ============================================================
static void process_audio(int16_t* buffer, size_t frames)
{
    if (!g_engine_running) {
        // Phase 1: output sine wave for testing
        generate_sine(buffer, frames);
        return;
    }

    // Phase 2+: full engine sound
    // Apply idle fluctuation
    EngineVoice::Params params = g_params;
    params.rpm += g_idle_fluct.update(params.rpm, static_cast<float>(frames) / AUDIO_SAMPLE_RATE);
    if (params.rpm < 500.0f)
        params.rpm = 500.0f;

    // Process engine voice
    sample_t engine_buf[AUDIO_BUFFER_SIZE];
    g_engine.process(params, engine_buf, frames);

    // Combustion pulses
    float effects_buf[AUDIO_BUFFER_SIZE] = {};
    g_combustion.process(effects_buf, frames, params.rpm, AUDIO_SAMPLE_RATE);

    // Mix
    g_mixer.clear(frames);
    g_mixer.add(engine_buf, frames, 1.0f);

    sample_t pulse_buf[AUDIO_BUFFER_SIZE];
    for (size_t i = 0; i < frames; ++i) {
        float s = effects_buf[i];
        if (s > 32767.0f)
            s = 32767.0f;
        if (s < -32768.0f)
            s = -32768.0f;
        pulse_buf[i] = static_cast<sample_t>(s);
    }
    g_mixer.add(pulse_buf, frames, 1.0f);
    g_mixer.finalize(buffer, frames);
}

// ============================================================
// DMA Interrupt Handler (I2S TX) - our own implementation
// BSP's audio.c is NOT compiled (it has player-specific logic)
// ============================================================
extern "C" void DMA1_Channel3_IRQHandler(void)
{
    if (dma_flag_get(DMA1_HDT3_FLAG)) {
        dma_flag_clear(DMA1_HDT3_FLAG);
        process_audio(&dma_buffer[0], AUDIO_BUFFER_SIZE);
    }
    if (dma_flag_get(DMA1_FDT3_FLAG)) {
        dma_flag_clear(DMA1_FDT3_FLAG);
        process_audio(&dma_buffer[AUDIO_BUFFER_SIZE], AUDIO_BUFFER_SIZE);
    }
}

// ============================================================
// System initialization
// ============================================================
static void system_init(void)
{
    // Clock is configured by system_clock_config() in startup

    // NVIC priority group
    nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);

    // Delay init (SysTick)
    delay_init();

    // PCA9555 (needed for audio enable + joystick)
    pca9555_init(PCA_I2C_CLKCTRL_100K);

    // Keys
    key_init();

    // Variable resistor (ADC for throttle)
    variable_resistor_init();

    // SDRAM
    sdram_init();

    // Audio: WM8988 + I2S + DMA
    // TODO: Implement I2S init based on at_surf_f437_board_audio.c
    // audio_init() is in BSP but has player dependencies
    // Phase 1: will add minimal I2S init here
}

// ============================================================
// Main
// ============================================================
int main(void)
{
    system_init();

    // Generate combustion pulse
    for (int i = 0; i < 512; ++i) {
        float t = static_cast<float>(i) / 44100.0f;
        float env = std::exp(-t * 150.0f);
        float wave = std::sin(t * 120.0f * 6.28f) * 0.6f
            + std::sin(t * 60.0f * 6.28f) * 0.4f;
        g_pulse_sample[i] = static_cast<sample_t>(wave * env * 12000.0f);
    }

    // Configure effects
    CombustionPulse::Config pc;
    pc.cylinders = 8;
    pc.volume = 0.2f;
    pc.rpm_fade_start = 3000.0f;
    pc.rpm_fade_end = 5000.0f;
    g_combustion.load_pulse(g_pulse_sample, 512);
    g_combustion.set_config(pc);

    IdleFluctuation::Config ic;
    ic.amplitude = 25.0f;
    ic.rate = 1.0f;
    ic.rpm_threshold = 1500.0f;
    g_idle_fluct.set_config(ic);

    g_mixer.set_master_volume(0.8f);

    // Start I2S DMA output (sine wave test in Phase 1)
    // The DMA interrupt will call process_audio() to fill buffers
    // TODO: Start DMA with dma_buffer, size = AUDIO_BUFFER_SIZE * 2

    // Main loop
    while (1) {
        // Read throttle from potentiometer
        // float throttle = analogRead(PA5) / 4095.0f;
        // TODO: Implement with ADC read

        // Read keys for shifting
        // TODO: ButtonEvent integration

        // Update physics
        // g_transmission.update(throttle, 0.001f); // 1ms tick
        // g_params.rpm = g_transmission.rpm();
        // g_params.throttle = throttle;
        // g_params.load = g_transmission.load();

        delay_ms(1); // 1kHz main loop
    }
}
