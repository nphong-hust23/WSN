/**
 ******************************************************************************
 * @file    platform.c
 * @brief   Implementation of Hardware Abstraction Layer (STM32 HAL).
 ******************************************************************************
 */
#include "platform.h"

/* ========================================================================== */
/* SHT3X I2C IMPLEMENTATION                                                   */
/* ========================================================================== */

int8_t SHT3x_Platform_I2C_Transmit(uint8_t address, uint8_t* p_data, uint16_t size, uint32_t timeout) {
    if (HAL_I2C_Master_Transmit(&hi2c1, address, p_data, size, timeout) == HAL_OK) {
        return 0;
    }
    return -1;
}

int8_t SHT3x_Platform_I2C_Receive(uint8_t address, uint8_t* buffer, uint16_t size, uint32_t timeout) {
    if (HAL_I2C_Master_Receive(&hi2c1, address, buffer, size, timeout) == HAL_OK) {
        return 0;
    }
    return -1;
}

void SHT3x_Platform_Delay_ms(uint32_t ms) {
    HAL_Delay(ms);
}

/* ========================================================================== */
/* LORA SPI & GPIO IMPLEMENTATION                                             */
/* ========================================================================== */

void LoRa_Platform_SPI_Init(void) {
    /* SPI initialization is already handled by MX_SPI1_Init() in main.c,
       so this wrapper can remain empty for STM32. */
}

uint8_t LoRa_Platform_SPI_Transfer(uint8_t byte) {
    uint8_t rx = 0;
    /* Transmit and receive simultanAeusly via SPI */
    HAL_SPI_TransmitReceive(&hspi1, &byte, &rx, 1, 10);
    return rx;
}

void LoRa_Platform_NSS_Low(void) {
    HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_RESET);
}

void LoRa_Platform_NSS_High(void) {
    HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_SET);
}

void LoRa_Platform_RESET_Low(void) {
    HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, GPIO_PIN_RESET);
}

void LoRa_Platform_RESET_High(void) {
    HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, GPIO_PIN_SET);
}

uint32_t LoRa_Platform_GetTickMs(void) {
    /* Returns the current system tick in milliseconds */
    return HAL_GetTick();
}

void LoRa_Platform_DelayMs(uint32_t ms) {
    HAL_Delay(ms);
}
