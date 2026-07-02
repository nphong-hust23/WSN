/**
 ******************************************************************************
 * @file    soil_sensor.c
 * @brief   Implementation of Soil Moisture calculation via ADC Polling.
 ******************************************************************************
 */
#include "soil_sensor.h"

/* Pointer to store the ADC hardware handle */
static ADC_HandleTypeDef* _soil_hadc = NULL;

void SoilSensor_Init(ADC_HandleTypeDef* hadc) {
    /* Store the ADC handle.
       No DMA or Interrupt needs to be started here. */
    _soil_hadc = hadc;
}

uint8_t SoilSensor_GetPercentage(void) {
    /* Safety check */
    if (_soil_hadc == NULL) {
        return 0;
    }

    uint32_t raw_val = 0;

    /* 1. Wake up the ADC and trigger a single conversion */
    HAL_ADC_Start(_soil_hadc);

    /* 2. Wait for conversion to complete (Timeout = 10ms) */
    if (HAL_ADC_PollForConversion(_soil_hadc, 10) == HAL_OK) {
        /* Read the converted 12-bit digital value */
        raw_val = HAL_ADC_GetValue(_soil_hadc);
    }

    /* 3. Stop the ADC immediately to save power */
    HAL_ADC_Stop(_soil_hadc);

    /* 4. Convert raw ADC value (0-4095) to physical voltage (0.0V - 3.3V) */
    float voltage = ((float)raw_val * SOIL_ADC_REF_VOLT) / SOIL_ADC_MAX_VAL;

    /* 5. Linearize the inverted voltage characteristics into percentage */
    float slope_denominator = SOIL_VOLTAGE_DRY - SOIL_VOLTAGE_WET;

    /* Prevent division by zero if calibration values are wrong */
    if (slope_denominator == 0.0f) {
        return 0;
    }

    float percent_calc = 100.0f * (SOIL_VOLTAGE_DRY - voltage) / slope_denominator;

    /* 6. Bound-check the result to ensure it stays within 0% - 100% */
    if (percent_calc > 100.0f) {
        percent_calc = 100.0f;
    }
    if (percent_calc < 0.0f) {
        percent_calc = 0.0f;
    }

    return (uint8_t)percent_calc;
}
