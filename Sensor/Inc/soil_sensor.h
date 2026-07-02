/**
 ******************************************************************************
 * @file    soil_sensor.h
 * @brief   Soil Moisture Sensor Driver (Single-Shot Polling Mode).
 *          Optimized for Low-Power Wireless Sensor Networks.
 ******************************************************************************
 */
#ifndef SOIL_SENSOR_H_
#define SOIL_SENSOR_H_

#include "main.h"

/* --- HARDWARE & RESOLUTION CONSTANTS --- */
#define SOIL_ADC_MAX_VAL      4095.0f   /* 12-bit ADC resolution */
#define SOIL_ADC_REF_VOLT     3.3f      /* STM32 Reference Voltage (V) */

/* --- CALIBRATION CONSTANTS --- */
/* Note: You must measure and update these voltages based on your real sensor! */
#define SOIL_VOLTAGE_DRY      2.80f     /* Output voltage when sensor is in dry air */
#define SOIL_VOLTAGE_WET      1.20f     /* Output voltage when sensor is submerged in water */

/* --- FUNCTION PROTOTYPES --- */
/**
 * @brief  Initialize the Soil Sensor module.
 * @param  hadc: Pointer to the ADC handle (e.g., &hadc1).
 */
void SoilSensor_Init(ADC_HandleTypeDef* hadc);

/**
 * @brief  Wake up ADC, take a single measurement, and return humidity percentage.
 * @return Soil moisture level from 0 to 100 (%RH).
 */
uint8_t SoilSensor_GetPercentage(void);

#endif /* SOIL_SENSOR_H_ */
