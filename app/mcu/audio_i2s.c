/**
 * ExhaustNote I2S Audio Output Driver
 * Based on AT-SURF-F437 BSP audio driver (WM8988 + I2S2 + DMA1_CH3)
 *
 * Audio path: MCU I2S2 (Master TX) → WM8988 (Slave) → Headphone/Line Out
 * MCLK: TMR8_CH1 (PC6) generates 12MHz clock for WM8988
 */

#include "audio_i2s.h"
#include "at32f435_437.h"
#include "at_surf_f437_board_audio.h"
#include "at_surf_f437_board_pca9555.h"
#include <stddef.h>

// DMA double buffer
#define AUDIO_DMA_BUFFER_SIZE 1024 // Total samples (512 per half)
#define AUDIO_HALF_BUFFER (AUDIO_DMA_BUFFER_SIZE / 2)

static int16_t g_dma_buffer[AUDIO_DMA_BUFFER_SIZE];
static audio_fill_callback_t g_fill_callback = NULL;

// WM8988 register configuration — matches BSP proven config (48kHz, Master, USB mode)
static const uint16_t wm8988_init_regs[] = {
    (WM8988_R15_RESET << 9) | 0x0000, // Reset all registers
    (WM8988_R0_LEFT_INPUT_VOLUME << 9) | 0x012F, // Left Input Volume
    (WM8988_R1_RIGHT_INPUT_VOLUME << 9) | 0x012F, // Right Input Volume
    (WM8988_R2_LOUT1_VOLUME << 9) | 0x0179, // LOUT1 (headphone) volume
    (WM8988_R3_ROUT1_VOLUME << 9) | 0x0179, // ROUT1 (headphone) volume
    (WM8988_R5_ADC_DAC_CONTROL << 9) | 0x0006, // De-emphasis 48kHz
    (WM8988_R7_AUDIO_INTERFACE << 9) | 0x0042, // I2S, 16-bit, Master mode
    (WM8988_R8_SAMPLE_RATE << 9) | 0x0081, // 48kHz, USB mode, CLKDIV2 (12MHz MCLK)
    (WM8988_R10_LEFT_DAC_VOLUME << 9) | 0x01FF, // Left DAC volume max
    (WM8988_R11_RIGHT_DAC_VOLUME << 9) | 0x01FF, // Right DAC volume max
    (WM8988_R12_BASS_CONTROL << 9) | 0x000F, // Bass control
    (WM8988_R13_TREBLE_CONTROL << 9) | 0x000F, // Treble control
    (WM8988_R23_ADDITIONAL_1_CONTROL << 9) | 0x00C2, // Additional Control 1 (LRCM, etc.)
    (WM8988_R31_ADC_INPUT_MODE << 9) | 0x0000, // ADC input mode
    (WM8988_R34_LEFT_OUT_MIX_1 << 9) | 0x0152, // Left mixer: DAC to output
    (WM8988_R35_LEFT_OUT_MIX_2 << 9) | 0x0050, // Left mixer 2
    (WM8988_R36_RIGHT_OUT_MIX_1 << 9) | 0x0052, // Right mixer 1
    (WM8988_R37_RIGHT_OUT_MIX_2 << 9) | 0x0150, // Right mixer: DAC to output
    (WM8988_R40_LOUT2_VOLUME << 9) | 0x01FF, // LOUT2 (speaker) volume max
    (WM8988_R41_ROUT2_VOLUME << 9) | 0x01FF, // ROUT2 (speaker) volume max
    (WM8988_R43_LOW_POWER_PALYBACK << 9) | 0x0008, // Low power playback
    (WM8988_R25_PWR_1_MGMT << 9) | 0x017C, // Power Management 1
    (WM8988_R26_PWR_2_MGMT << 9) | 0x01F8, // Power Management 2
};

/**
 * Generate MCLK via TMR8 PWM on PC6 (12MHz for WM8988)
 */
static void mclk_init(void)
{
    gpio_init_type gpio_init_struct;
    tmr_output_config_type tmr_oc_init_structure;
    // 288MHz / 12MHz = 24, prescaler = 23, period = 1 → 50% duty
    uint16_t prescaler_value = (uint16_t)(system_core_clock / 24000000) - 1;

    crm_periph_clock_enable(CRM_TMR8_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);

    gpio_default_para_init(&gpio_init_struct);
    gpio_pin_mux_config(GPIOC, GPIO_PINS_SOURCE6, GPIO_MUX_3);
    gpio_init_struct.gpio_pins = GPIO_PINS_6;
    gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init(GPIOC, &gpio_init_struct);

    tmr_base_init(TMR8, 1, prescaler_value);
    tmr_cnt_dir_set(TMR8, TMR_COUNT_UP);

    tmr_output_default_para_init(&tmr_oc_init_structure);
    tmr_oc_init_structure.oc_mode = TMR_OUTPUT_CONTROL_PWM_MODE_A;
    tmr_oc_init_structure.oc_polarity = TMR_OUTPUT_ACTIVE_HIGH;
    tmr_oc_init_structure.oc_output_state = TRUE;
    tmr_output_channel_config(TMR8, TMR_SELECT_CHANNEL_1, &tmr_oc_init_structure);
    tmr_channel_value_set(TMR8, TMR_SELECT_CHANNEL_1, 1);
    tmr_output_channel_buffer_enable(TMR8, TMR_SELECT_CHANNEL_1, TRUE);

    tmr_counter_enable(TMR8, TRUE);
    tmr_output_enable(TMR8, TRUE);
}

/**
 * Write a register to WM8988 via I2C (2-byte raw transmit, same as BSP)
 * WM8988 protocol: [7-bit reg addr | 1-bit data MSB] [8-bit data LSB]
 */
static void wm8988_write_reg(uint16_t reg_data)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(reg_data >> 8);
    buf[1] = (uint8_t)(reg_data & 0xFF);

    i2c_master_transmit(&hi2c_pca, WM8988_I2C_ADDR_CSB_LOW, buf, 2, 0xFFFFFF);
}

/**
 * Initialize WM8988 codec registers
 */
static void wm8988_init(void)
{
    uint32_t count = sizeof(wm8988_init_regs) / sizeof(wm8988_init_regs[0]);
    for (uint32_t i = 0; i < count; i++) {
        wm8988_write_reg(wm8988_init_regs[i]);
        // Small delay between register writes
        for (volatile int d = 0; d < 1000; d++)
            ;
    }
}

/**
 * Initialize I2S2 + DMA1_CH3 for audio TX
 */
static void i2s_dma_init(void)
{
    gpio_init_type gpio_init_struct;
    dma_init_type dma_init_struct;
    i2s_init_type i2s_init_struct;

    // Enable clocks
    crm_periph_clock_enable(I2S_CK_GPIO_CLK, TRUE);
    crm_periph_clock_enable(I2S_WS_GPIO_CLK, TRUE);
    crm_periph_clock_enable(I2S_SD_OUT_GPIO_CLK, TRUE);
    crm_periph_clock_enable(I2S_DMAx_CLK, TRUE);
    crm_periph_clock_enable(I2S_SPIx_CLK, TRUE);

    // GPIO: I2S pins
    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;

    // WS (LRCK) - PB12
    gpio_init_struct.gpio_pull = GPIO_PULL_UP;
    gpio_init_struct.gpio_pins = I2S_WS_GPIO_PIN;
    gpio_init(I2S_WS_GPIO_PORT, &gpio_init_struct);
    gpio_pin_mux_config(I2S_WS_GPIO_PORT, I2S_WS_GPIO_PINS_SOURCE, I2S_WS_GPIO_MUX);

    // CK (BCLK) - PB13
    gpio_init_struct.gpio_pull = GPIO_PULL_DOWN;
    gpio_init_struct.gpio_pins = I2S_CK_GPIO_PIN;
    gpio_init(I2S_CK_GPIO_PORT, &gpio_init_struct);
    gpio_pin_mux_config(I2S_CK_GPIO_PORT, I2S_CK_GPIO_PINS_SOURCE, I2S_CK_GPIO_MUX);

    // SD_OUT (DOUT) - PB15
    gpio_init_struct.gpio_pull = GPIO_PULL_DOWN;
    gpio_init_struct.gpio_pins = I2S_SD_OUT_GPIO_PIN;
    gpio_init(I2S_SD_OUT_GPIO_PORT, &gpio_init_struct);
    gpio_pin_mux_config(I2S_SD_OUT_GPIO_PORT, I2S_SD_OUT_GPIO_PINS_SOURCE, I2S_SD_OUT_GPIO_MUX);

    // DMA1 Channel 3: I2S TX (circular mode, half/full transfer interrupts)
    dma_reset(I2S_DMA_TX_CHANNEL);
    dma_default_para_init(&dma_init_struct);
    dma_init_struct.buffer_size = AUDIO_DMA_BUFFER_SIZE;
    dma_init_struct.direction = DMA_DIR_MEMORY_TO_PERIPHERAL;
    dma_init_struct.memory_base_addr = (uint32_t)g_dma_buffer;
    dma_init_struct.memory_data_width = DMA_MEMORY_DATA_WIDTH_HALFWORD;
    dma_init_struct.memory_inc_enable = TRUE;
    dma_init_struct.peripheral_base_addr = (uint32_t)I2S_DT_ADDRESS;
    dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_HALFWORD;
    dma_init_struct.peripheral_inc_enable = FALSE;
    dma_init_struct.priority = DMA_PRIORITY_HIGH;
    dma_init_struct.loop_mode_enable = TRUE;
    dma_init(I2S_DMA_TX_CHANNEL, &dma_init_struct);

    // Enable half-transfer and full-transfer interrupts
    dma_interrupt_enable(I2S_DMA_TX_CHANNEL, DMA_HDT_INT, TRUE);
    dma_interrupt_enable(I2S_DMA_TX_CHANNEL, DMA_FDT_INT, TRUE);
    nvic_irq_enable(DMA1_Channel3_IRQn, 2, 0); // Priority 2 (I2C=0, SDIO=1 can preempt)

    // I2S2: Master TX, 16-bit, Phillips, 44.1kHz
    spi_i2s_reset(I2S_SPIx);
    i2s_default_para_init(&i2s_init_struct);
    i2s_init_struct.audio_protocol = I2S_AUDIO_PROTOCOL_PHILLIPS;
    i2s_init_struct.data_channel_format = I2S_DATA_16BIT_CHANNEL_16BIT;
    i2s_init_struct.mclk_output_enable = FALSE; // We use TMR8 for MCLK
    i2s_init_struct.audio_sampling_freq = I2S_AUDIO_FREQUENCY_48K;
    i2s_init_struct.clock_polarity = I2S_CLOCK_POLARITY_LOW;
    i2s_init_struct.operation_mode = I2S_MODE_SLAVE_TX; // WM8988 is Master (provides BCLK/LRCK)
    i2s_init(I2S_SPIx, &i2s_init_struct);

    // Connect DMA to I2S TX
    spi_i2s_dma_transmitter_enable(I2S_SPIx, TRUE);
    dmamux_enable(I2S_DMAx, TRUE);
    dmamux_init(I2S_DMA_TX_DMAMUX_CHANNEL, I2S_DMA_TX_DMAREQ);
}

// ============================================================
// Public API
// ============================================================

void audio_i2s_init(audio_fill_callback_t callback)
{
    g_fill_callback = callback;

    // Clear buffer
    for (int i = 0; i < AUDIO_DMA_BUFFER_SIZE; i++)
        g_dma_buffer[i] = 0;

    // Enable audio PA via PCA9555 (active low)
    pca9555_out_mode_config(PCA_IO0_PINS_2);
    pca9555_bits_reset(PCA_IO0_PINS_2);

    // Initialize MCLK (12MHz on PC6)
    mclk_init();

    // Initialize WM8988 via I2C
    wm8988_init();

    // Initialize I2S + DMA
    i2s_dma_init();
}

void audio_i2s_start(void)
{
    // Pre-fill buffer
    if (g_fill_callback) {
        g_fill_callback(&g_dma_buffer[0], AUDIO_HALF_BUFFER);
        g_fill_callback(&g_dma_buffer[AUDIO_HALF_BUFFER], AUDIO_HALF_BUFFER);
    }

    // Start DMA and I2S
    dma_channel_enable(I2S_DMA_TX_CHANNEL, TRUE);
    i2s_enable(I2S_SPIx, TRUE);
}

void audio_i2s_stop(void)
{
    i2s_enable(I2S_SPIx, FALSE);
    dma_channel_enable(I2S_DMA_TX_CHANNEL, FALSE);
}

void audio_i2s_set_volume(uint8_t volume)
{
    uint16_t vol = (uint16_t)volume | 0x0100; // Update bit
    wm8988_write_reg((WM8988_R2_LOUT1_VOLUME << 9) | vol);
    wm8988_write_reg((WM8988_R3_ROUT1_VOLUME << 9) | vol);
}

// ============================================================
// DMA Interrupt Handler
// ============================================================

void DMA1_Channel3_IRQHandler(void)
{
    if (dma_flag_get(DMA1_HDT3_FLAG)) {
        dma_flag_clear(DMA1_HDT3_FLAG);
        if (g_fill_callback)
            g_fill_callback(&g_dma_buffer[0], AUDIO_HALF_BUFFER);
    }
    if (dma_flag_get(DMA1_FDT3_FLAG)) {
        dma_flag_clear(DMA1_FDT3_FLAG);
        if (g_fill_callback)
            g_fill_callback(&g_dma_buffer[AUDIO_HALF_BUFFER], AUDIO_HALF_BUFFER);
    }
}
