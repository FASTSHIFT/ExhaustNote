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
#ifndef __ADC_H
#define __ADC_H

/*********************
 *      INCLUDES
 *********************/

#include "mcu_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef enum {
    ADC_DMA_RES_OK = 0,
    ADC_DMA_RES_NOT_ADC_CHANNEL = -1,
    ADC_DMA_RES_DUPLICATE_REGISTRATION = -2,
    ADC_DMA_RES_MAX_NUM_OF_REGISTRATIONS_EXCEEDED = -3,
} ADC_DMA_Res_Type;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief  Initialize the ADCx peripheral.
 * @param  ADCx: Pointer to ADCx peripheral.
 */
void ADCx_Init(adc_type* ADCx);

/**
 * @brief  Get the ADCx value.
 * @param  ADCx: Pointer to ADCx peripheral.
 * @param  channel: ADC channel number.
 * @return ADC value.
 */
uint16_t ADCx_GetValue(adc_type* ADCx, uint8_t channel);

/**
 * @brief  Initialize the ADC DMA.
 */
void ADC_DMA_Init(void);

/**
 * @brief  Get the ADC value from the registered channel.
 * @param  channel: ADC channel number.
 * @return ADC value.
 */
uint16_t ADC_DMA_GetValue(uint8_t channel);

/**
 * @brief  Register an ADC channel for DMA conversion.
 * @param  channel: ADC channel number.
 * @return ADC_DMA_Res_Type: Result of the registration.
 */
ADC_DMA_Res_Type ADC_DMA_Register(uint8_t channel);

/**
 * @brief  Get the number of registered ADC channels.
 * @return Number of registered ADC channels.
 */
uint8_t ADC_DMA_GetRegisterCount(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __ADC_H */
