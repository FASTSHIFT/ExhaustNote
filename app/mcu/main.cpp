/**
 * ExhaustNote MCU Firmware - AT32F437 Entry Point
 *
 * Architecture:
 *   system_init() → sd_mount() → car_load() → car_apply() → audio_i2s_start()
 *   Main loop: read throttle ADC → update transmission physics → update params
 *   ISR (DMA): process_audio() callback fills I2S buffer in real-time
 *
 * Modules:
 *   audio_i2s.c/h  — I2S DMA driver (WM8988 codec)
 *   sd_loader.c/h  — SD card + FatFS + WAV file reader
 *   car_loader.h/cpp — JSON config parser + EngineVoice setup
 */

#include "at32f435_437.h"

// BSP and driver includes
extern "C" {
#include "at_surf_f437_board_key.h"
#include "at_surf_f437_board_pca9555.h"
#include "at_surf_f437_board_sdram.h"
#include "at_surf_f437_board_variable_resistor.h"
#include "audio_i2s.h"
#include "sd_loader.h"
}

// App modules
#include "car_loader.h"

// Core engine includes
#include "core/engine_effects.h"
#include "core/engine_voice.h"
#include "core/mixer.h"
#include "core/transmission.h"
#include "core/types.h"

#include <cmath>
#include <cstring>

#include "Arduino.h"

using namespace exhaust;

// ============================================================
// Constants
// ============================================================
#define AUDIO_BUFFER_SIZE 512 // samples per half-buffer (DMA callback)
#define AUDIO_SAMPLE_RATE 48000 // Matches WM8988 config

// SDRAM: 8MB at 0xC0000000 for WAV sample storage
#define SDRAM_BASE ((int16_t*)0xC0000000)
#define SDRAM_SIZE_SAMPLES (8 * 1024 * 1024 / 2) // 4M samples (8MB / 2 bytes)

// ============================================================
// Engine state (global — accessed from ISR)
// ============================================================
static EngineVoice g_engine;
static Mixer g_mixer;
static CombustionPulse g_combustion;
static IdleFluctuation g_idle_fluct;
static Transmission g_transmission;
static EngineVoice::Params g_params;
static bool g_engine_running = false;

// Combustion pulse sample (synthesized at startup)
static sample_t g_pulse_sample[512];

// SDRAM memory pool for WAV samples
static SdramPool g_sdram_pool;
static LoadedCar g_car;

// ============================================================
// Sine wave fallback (500ms beep then silence)
// ============================================================
static float sine_phase = 0.0f;
static uint32_t sine_samples_played = 0;
static const uint32_t SINE_DURATION_SAMPLES = AUDIO_SAMPLE_RATE / 2; // 500ms

static void generate_sine(int16_t* buffer, size_t frames)
{
    const float phase_inc = 1000.0f / static_cast<float>(AUDIO_SAMPLE_RATE);
    for (size_t i = 0; i < frames; ++i) {
        if (sine_samples_played < SINE_DURATION_SAMPLES) {
            float sample = std::sin(sine_phase * 6.2831853f) * 16000.0f;
            buffer[i] = static_cast<int16_t>(sample);
            sine_phase += phase_inc;
            if (sine_phase >= 1.0f)
                sine_phase -= 1.0f;
            sine_samples_played++;
        } else {
            buffer[i] = 0;
        }
    }
}

// ============================================================
// Audio processing (called from DMA ISR via audio_i2s callback)
// ============================================================
static volatile uint32_t g_isr_cycles = 0; // DWT cycle count of last ISR
static volatile uint32_t g_isr_count = 0;

static void process_audio(int16_t* buffer, uint32_t frames)
{
    uint32_t t0 = DWT->CYCCNT;
    (void)t0; // used at end
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

    // Static buffers — avoid stack allocation in ISR
    static sample_t engine_buf[AUDIO_BUFFER_SIZE];
    static float effects_buf[AUDIO_BUFFER_SIZE];

    // Process engine voice
    g_engine.process(params, engine_buf, frames);

    // Combustion pulses (clear first)
    memset(effects_buf, 0, frames * sizeof(float));
    g_combustion.process(effects_buf, frames, params.rpm, AUDIO_SAMPLE_RATE);

    // Mix engine + effects → output
    g_mixer.clear(frames);
    g_mixer.add(engine_buf, frames, 1.0f);

    // Convert float effects to int16 and add
    for (size_t i = 0; i < frames; ++i) {
        float s = effects_buf[i];
        s = s > 32767.0f ? 32767.0f : (s < -32768.0f ? -32768.0f : s);
        engine_buf[i] = static_cast<sample_t>(s); // reuse buffer
    }
    g_mixer.add(engine_buf, frames, 1.0f);
    g_mixer.finalize(buffer, frames);

    g_isr_cycles = DWT->CYCCNT - t0;
    g_isr_count++;
}

// DMA IRQ handler is in audio_i2s.c — it calls process_audio() via callback

// ============================================================
// I2C2 (PCA9555) IRQ Handlers — required for DMA-based I2C
// ============================================================
extern "C" {
void DMA1_Channel1_IRQHandler(void)
{
    i2c_dma_tx_irq_handler(&hi2c_pca);
}

void DMA1_Channel2_IRQHandler(void)
{
    i2c_dma_rx_irq_handler(&hi2c_pca);
}

void I2C2_EVT_IRQHandler(void)
{
    i2c_evt_irq_handler(&hi2c_pca);
}

void I2C2_ERR_IRQHandler(void)
{
    i2c_err_irq_handler(&hi2c_pca);
}
} // extern "C"

// ============================================================
// System initialization
// ============================================================
extern "C" void Core_Init(void);

static void system_init(void)
{
    // Core_Init: system_clock_config() + NVIC + DWT + Delay + ADC
    Core_Init();

    // Serial (after clock is 288MHz)
    Serial.begin(115200);
    Serial.println("\n[ExhaustNote] Boot OK");
    Serial.print("  SystemCoreClock = ");
    Serial.println(system_core_clock);

    // SD card (before PCA9555, matching audio demo order)
    Serial.print("[INIT] SD card...");
    int sd_ret = sd_mount();
    if (sd_ret == 0) {
        Serial.println(" OK");
    } else {
        Serial.print(" FAIL (");
        Serial.print(-sd_ret);
        Serial.println(")");
    }

    // PCA9555 (needed for audio enable + joystick)
    Serial.print("[INIT] PCA9555...");
    pca9555_init(PCA_I2C_CLKCTRL_100K);
    Serial.println(" OK");

    // Keys
    key_init();

    // Variable resistor (ADC for throttle)
    variable_resistor_init();

    // SDRAM
    Serial.print("[INIT] SDRAM...");
    sdram_init();
    Serial.println(" OK");

    // Audio: WM8988 + I2S + DMA (our own driver)
    Serial.print("[INIT] Audio (WM8988 + I2S + DMA)...");
    audio_i2s_init(process_audio);
    Serial.println(" OK");
}

// ============================================================
// Load car from SD card
// ============================================================
static bool load_car_from_sd(const char* car_dir)
{
    // Initialize SDRAM pool
    g_sdram_pool.base = SDRAM_BASE;
    g_sdram_pool.capacity = SDRAM_SIZE_SAMPLES;
    g_sdram_pool.reset();

    Serial.print("[LOAD] Loading car: ");
    Serial.println(car_dir);

    if (!car_load(car_dir, g_sdram_pool, g_car)) {
        Serial.println("[LOAD] FAILED to load car!");
        return false;
    }

    Serial.print("  Onload layers: ");
    Serial.println(g_car.num_onload);
    Serial.print("  Offload layers: ");
    Serial.println(g_car.num_offload);
    Serial.print("  SDRAM used: ");
    Serial.print(g_sdram_pool.used * 2 / 1024);
    Serial.println(" KB");

    // Generate combustion pulse
    for (int i = 0; i < 512; ++i) {
        float t = static_cast<float>(i) / 48000.0f;
        float env = std::exp(-t * 150.0f);
        float wave = std::sin(t * 120.0f * 6.28f) * 0.6f + std::sin(t * 60.0f * 6.28f) * 0.4f;
        g_pulse_sample[i] = static_cast<sample_t>(wave * env * 12000.0f);
    }
    g_combustion.load_pulse(g_pulse_sample, 512);

    // Apply car config to engine
    car_apply(g_car, g_engine, g_transmission, g_combustion, g_idle_fluct);
    g_mixer.set_master_volume(0.8f);

    Serial.println("[LOAD] Car loaded OK!");
    return true;
}

// ============================================================
// Input — ButtonEvent + analogRead
// ============================================================
#include "ButtonEvent.h"

#define PIN_THROTTLE PA5 // Variable resistor (ADC channel 5)
#define PIN_KEY1 PA0 // Shift up (active high, pull-down)
#define PIN_KEY2 PC13 // Shift down (active high, pull-down)

static ButtonEvent g_btn_up;
static ButtonEvent g_btn_down;

static void on_button_event(ButtonEvent* btn, ButtonEvent::Event_t event)
{
    if (event != ButtonEvent::EVENT_CLICKED)
        return;

    if (btn == &g_btn_up) {
        g_transmission.shift_up();
        Serial.print("[GEAR] UP -> ");
        Serial.println(g_transmission.gear());
    } else if (btn == &g_btn_down) {
        g_transmission.shift_down();
        Serial.print("[GEAR] DOWN -> ");
        Serial.println(g_transmission.gear());
    }
}

static uint32_t tick_getter(void)
{
    return millis();
}

// ============================================================
// Main
// ============================================================
int main(void)
{
    system_init();

    // Configure input pins
    pinMode(PIN_THROTTLE, INPUT_ANALOG_DMA);
    pinMode(PIN_KEY1, INPUT_PULLDOWN);
    pinMode(PIN_KEY2, INPUT_PULLDOWN);
    ADC_DMA_Init();

    // Setup ButtonEvent
    ButtonEvent::setTickGetterCallback(tick_getter);
    g_btn_up.setEventCallback(on_button_event);
    g_btn_down.setEventCallback(on_button_event);

    // Try to load car from SD card
    const char* car_dir = "1:/ExhaustNote/ferrari_458";
    bool car_ok = load_car_from_sd(car_dir);

    // Start audio output
    Serial.print("[INIT] Starting I2S DMA...");
    audio_i2s_start();
    Serial.println(" OK");

    if (car_ok) {
        g_engine_running = true;
        g_params.rpm = g_car.rpm_idle;
        g_params.throttle = 0.0f;
        g_params.load = 0.0f;
        Serial.println("[ExhaustNote] Engine running!");
    } else {
        Serial.println("[ExhaustNote] No car loaded, sine wave fallback.");
    }

    // Main loop — time-based scheduling via millis()
    uint32_t last_physics_ms = millis();
    uint32_t last_button_ms = millis();
    uint32_t last_print_ms = millis();

    while (1) {
        uint32_t now = millis();

        // Button scan @ 100Hz (every 10ms)
        if (now - last_button_ms >= 10) {
            last_button_ms = now;
            g_btn_up.monitor(digitalRead(PIN_KEY1) == HIGH);
            g_btn_down.monitor(digitalRead(PIN_KEY2) == HIGH);
        }

        // Physics update @ 1kHz (every 1ms)
        if (now - last_physics_ms >= 1) {
            float dt = (now - last_physics_ms) * 0.001f;
            last_physics_ms = now;

            if (g_engine_running) {
                // Read throttle (12-bit ADC DMA -> 0.0~1.0)
                uint16_t adc_raw = analogRead_DMA(PIN_THROTTLE);
                float throttle = static_cast<float>(adc_raw) / 4095.0f;

                // Update transmission physics
                g_transmission.update(throttle, dt);

                // Update audio params (read by ISR)
                g_params.rpm = g_transmission.rpm();
                g_params.throttle = throttle;
                g_params.load = g_transmission.load();
            }
        }

        // Status print @ 0.5Hz (every 2s)
        if (now - last_print_ms >= 2000) {
            last_print_ms = now;
            // ISR CPU usage: cycles / budget (budget = 288MHz * 10.67ms = 3,072,000)
            uint32_t pct = g_isr_cycles * 100 / 3072000;
            Serial.print("[ALIVE] rpm=");
            Serial.print((int)g_params.rpm);
            Serial.print(" thr=");
            Serial.print((int)(g_params.throttle * 100));
            Serial.print(" gear=");
            Serial.print(g_transmission.gear());
            Serial.print(" isr=");
            Serial.print(g_isr_cycles);
            Serial.print("cy(");
            Serial.print(pct);
            Serial.println("%)");
        }
    }
}
