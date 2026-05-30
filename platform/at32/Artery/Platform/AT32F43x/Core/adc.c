/*
 * MIT License
 * Copyright (c) 2017 - 2025 _VIFEXTech
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*********************
 *      INCLUDES
 *********************/

#include "adc.h"
#include <stdbool.h>

/*********************
 *      DEFINES
 *********************/

#define ADC_DMA_REGMAX 18

#define IS_ADC_CHANNEL(channel) (channel <= ADC_CHANNEL_18)

/**********************
 *      TYPEDEFS
 **********************/

typedef struct
{
    uint8_t regCnt;
    uint8_t regChannelList[ADC_DMA_REGMAX];
    uint16_t convertedValue[ADC_DMA_REGMAX];
} ADC_DMA_Context_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static int16_t searchRegChannel(uint8_t channel);

/**********************
 *  STATIC VARIABLES
 **********************/

static ADC_DMA_Context_t ADC_DMA_Context = { 0 };

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void ADCx_Init(adc_type* ADCx)
{
    if (ADCx == ADC1) {
        crm_periph_clock_enable(CRM_ADC1_PERIPH_CLOCK, TRUE);
    } else if (ADCx == ADC2) {
        crm_periph_clock_enable(CRM_ADC2_PERIPH_CLOCK, TRUE);
    } else if (ADCx == ADC3) {
        crm_periph_clock_enable(CRM_ADC3_PERIPH_CLOCK, TRUE);
    } else {
        return;
    }

    adc_common_config_type adc_common_struct;
    adc_common_default_para_init(&adc_common_struct);
    adc_common_struct.combine_mode = ADC_INDEPENDENT_MODE;
    adc_common_struct.div = ADC_HCLK_DIV_4;
    adc_common_struct.common_dma_mode = ADC_COMMON_DMAMODE_DISABLE;
    adc_common_struct.common_dma_request_repeat_state = FALSE;
    adc_common_struct.sampling_interval = ADC_SAMPLING_INTERVAL_5CYCLES;
    adc_common_struct.tempervintrv_state = FALSE;
    adc_common_struct.vbat_state = FALSE;
    adc_common_config(&adc_common_struct);

    adc_base_config_type adc_base_struct;
    adc_base_default_para_init(&adc_base_struct);
    adc_base_struct.sequence_mode = FALSE;
    adc_base_struct.repeat_mode = FALSE;
    adc_base_struct.data_align = ADC_RIGHT_ALIGNMENT;
    adc_base_struct.ordinary_channel_length = 1;
    adc_base_config(ADCx, &adc_base_struct);
    adc_resolution_set(ADCx, ADC_RESOLUTION_12B);

    adc_ordinary_conversion_trigger_set(ADCx, ADC_ORDINARY_TRIG_TMR1CH1, ADC_ORDINARY_TRIG_EDGE_NONE);

    adc_dma_mode_enable(ADCx, FALSE);
    adc_dma_request_repeat_enable(ADCx, FALSE);
    adc_interrupt_enable(ADCx, ADC_OCCO_INT, FALSE);

    adc_enable(ADCx, TRUE);
    while (adc_flag_get(ADCx, ADC_RDY_FLAG) == RESET) { }

    /* adc calibration */
    adc_calibration_init(ADCx);
    while (adc_calibration_init_status_get(ADCx)) { }

    adc_calibration_start(ADCx);
    while (adc_calibration_status_get(ADCx)) { }
}

uint16_t ADCx_GetValue(adc_type* ADCx, uint8_t channel)
{
    adc_ordinary_channel_set(ADCx, (adc_channel_select_type)channel, 1, ADC_SAMPLETIME_47_5);

    adc_ordinary_software_trigger_enable(ADCx, TRUE);
    while (!adc_flag_get(ADCx, ADC_OCCE_FLAG)) { }

    return adc_ordinary_conversion_data_get(ADCx);
}

void ADC_DMA_Init(void)
{
    crm_periph_clock_enable(CRM_ADC1_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_DMA1_PERIPH_CLOCK, TRUE);

    dma_reset(DMA1_CHANNEL1);

    dma_init_type dma_init_structure;
    dma_default_para_init(&dma_init_structure);
    dma_init_structure.buffer_size = ADC_DMA_Context.regCnt;
    dma_init_structure.direction = DMA_DIR_PERIPHERAL_TO_MEMORY;
    dma_init_structure.memory_base_addr = (uint32_t)ADC_DMA_Context.convertedValue;
    dma_init_structure.memory_data_width = DMA_MEMORY_DATA_WIDTH_HALFWORD;
    dma_init_structure.memory_inc_enable = TRUE;
    dma_init_structure.peripheral_base_addr = (uint32_t)(&(ADC1->odt));
    dma_init_structure.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_HALFWORD;
    dma_init_structure.peripheral_inc_enable = FALSE;
    dma_init_structure.priority = DMA_PRIORITY_HIGH;
    dma_init_structure.loop_mode_enable = TRUE;

    dma_init(DMA1_CHANNEL1, &dma_init_structure);

    dmamux_enable(DMA1, TRUE);
    dmamux_init(DMA1MUX_CHANNEL1, DMAMUX_DMAREQ_ID_ADC1);

    adc_reset();

    adc_common_config_type adc_common_struct;
    adc_common_default_para_init(&adc_common_struct);
    adc_common_struct.combine_mode = ADC_INDEPENDENT_MODE;
    adc_common_struct.div = ADC_HCLK_DIV_4;
    adc_common_struct.common_dma_mode = ADC_COMMON_DMAMODE_DISABLE;
    adc_common_struct.common_dma_request_repeat_state = FALSE;
    adc_common_struct.sampling_interval = ADC_SAMPLING_INTERVAL_5CYCLES;
    adc_common_struct.tempervintrv_state = FALSE;
    adc_common_struct.vbat_state = FALSE;
    adc_common_config(&adc_common_struct);

    adc_base_config_type adc_base_struct;
    adc_base_default_para_init(&adc_base_struct);
    adc_base_struct.sequence_mode = TRUE;
    adc_base_struct.repeat_mode = TRUE;
    adc_base_struct.data_align = ADC_RIGHT_ALIGNMENT;
    adc_base_struct.ordinary_channel_length = ADC_DMA_Context.regCnt;
    adc_base_config(ADC1, &adc_base_struct);
    adc_resolution_set(ADC1, ADC_RESOLUTION_12B);

    for (uint8_t index = 0; index < ADC_DMA_Context.regCnt; index++) {
        adc_ordinary_channel_set(
            ADC1,
            (adc_channel_select_type)ADC_DMA_Context.regChannelList[index],
            index + 1,
            ADC_SAMPLETIME_47_5);
    }

    adc_ordinary_conversion_trigger_set(ADC1, ADC_ORDINARY_TRIG_TMR1CH1, ADC_ORDINARY_TRIG_EDGE_NONE);

    adc_dma_mode_enable(ADC1, TRUE);

    adc_dma_request_repeat_enable(ADC1, TRUE);

    adc_interrupt_enable(ADC1, ADC_OCCO_INT, FALSE);

    adc_enable(ADC1, TRUE);
    while (adc_flag_get(ADC1, ADC_RDY_FLAG) == RESET) { }

    adc_calibration_init(ADC1);
    while (adc_calibration_init_status_get(ADC1)) { }

    adc_calibration_start(ADC1);
    while (adc_calibration_status_get(ADC1)) { }
    dma_channel_enable(DMA1_CHANNEL1, TRUE);
    adc_ordinary_software_trigger_enable(ADC1, TRUE);
}

uint16_t ADC_DMA_GetValue(uint8_t channel)
{
    if (!IS_ADC_CHANNEL(channel)) {
        return 0;
    }

    int16_t index = searchRegChannel(channel);
    if (index < 0) {
        return 0;
    }

    return ADC_DMA_Context.convertedValue[index];
}

ADC_DMA_Res_Type ADC_DMA_Register(uint8_t channel)
{
    if (!IS_ADC_CHANNEL(channel)) {
        return ADC_DMA_RES_NOT_ADC_CHANNEL;
    }

    if (searchRegChannel(channel) >= 0) {
        return ADC_DMA_RES_DUPLICATE_REGISTRATION;
    }

    if (ADC_DMA_Context.regCnt >= ADC_DMA_REGMAX) {
        return ADC_DMA_RES_MAX_NUM_OF_REGISTRATIONS_EXCEEDED;
    }

    ADC_DMA_Context.regChannelList[ADC_DMA_Context.regCnt] = channel;
    ADC_DMA_Context.regCnt++;
    return ADC_DMA_RES_OK;
}

uint8_t ADC_DMA_GetRegisterCount(void)
{
    return ADC_DMA_Context.regCnt;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static int16_t searchRegChannel(uint8_t channel)
{
    for (int16_t index = 0; index < ADC_DMA_Context.regCnt; index++) {
        if (channel == ADC_DMA_Context.regChannelList[index]) {
            return index;
        }
    }
    return -1;
}
