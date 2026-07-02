/**
 ******************************************************************************
 * @file    platform.h
 * @brief   Hardware Abstraction Layer for I2C, SPI, and System Timing.
 ******************************************************************************
 */
#ifndef PLATFORM_H_
#define PLATFORM_H_

#include "main.h"

/* Extern peripheral handles defined in main.c */
extern I2C_HandleTypeDef hi2c1;
extern SPI_HandleTypeDef hspi1;

/* ========================================================================== */
/* SHT3X SENSOR WRAPPERS (I2C)                                                */
/* ========================================================================== */
int8_t SHT3x_Platform_I2C_Transmit(uint8_t address, uint8_t* p_data, uint16_t size, uint32_t timeout);
int8_t SHT3x_Platform_I2C_Receive(uint8_t address, uint8_t* buffer, uint16_t size, uint32_t timeout);
void   SHT3x_Platform_Delay_ms(uint32_t ms);

/* ========================================================================== */
/* LORA SX1278 WRAPPERS (SPI & GPIO)                                          */
/* ========================================================================== */
void     LoRa_Platform_SPI_Init(void);
uint8_t  LoRa_Platform_SPI_Transfer(uint8_t byte);
void     LoRa_Platform_NSS_Low(void);
void     LoRa_Platform_NSS_High(void);
void     LoRa_Platform_RESET_Low(void);
void     LoRa_Platform_RESET_High(void);
uint32_t LoRa_Platform_GetTickMs(void);
void     LoRa_Platform_DelayMs(uint32_t ms);

#endif /* PLATFORM_H_ */
