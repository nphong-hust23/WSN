/**
 ******************************************************************************
 * @file    sht3x.h
 * @brief   Complete SHT3x-DIS Temperature & Humidity Sensor Driver.
 *          Supports Single Shot, Periodic, and ART measurement modes.
 ******************************************************************************
 */
#ifndef SHT3X_H_
#define SHT3X_H_

#include <stdint.h>

/* --- I2C DEVICE ADDRESSES --- */
#define SHT3X_I2C_ADDR_GND          (0x44 << 1) /* ADDR pin connected to GND */
#define SHT3X_I2C_ADDR_VDD          (0x45 << 1) /* ADDR pin connected to VDD */

/* --- SHT3X 16-BIT COMMANDS --- */
/* Single Shot Mode (CS = Clock Stretching) */
#define SHT3X_CMD_MEAS_CS_HIGH      0x2C06
#define SHT3X_CMD_MEAS_CS_MED       0x2C0D
#define SHT3X_CMD_MEAS_CS_LOW       0x2C10
#define SHT3X_CMD_MEAS_HIGH         0x2400
#define SHT3X_CMD_MEAS_MED          0x240B
#define SHT3X_CMD_MEAS_LOW          0x2416

/* Periodic Data Acquisition Mode */
#define SHT3X_CMD_PERI_0_5_HIGH     0x2032
#define SHT3X_CMD_PERI_0_5_MED      0x2024
#define SHT3X_CMD_PERI_0_5_LOW      0x202F
#define SHT3X_CMD_PERI_1_HIGH       0x2130
#define SHT3X_CMD_PERI_1_MED        0x2126
#define SHT3X_CMD_PERI_1_LOW        0x212D
#define SHT3X_CMD_PERI_2_HIGH       0x2236
#define SHT3X_CMD_PERI_2_MED        0x2220
#define SHT3X_CMD_PERI_2_LOW        0x222B
#define SHT3X_CMD_PERI_4_HIGH       0x2334
#define SHT3X_CMD_PERI_4_MED        0x2322
#define SHT3X_CMD_PERI_4_LOW        0x2329
#define SHT3X_CMD_PERI_10_HIGH      0x2737
#define SHT3X_CMD_PERI_10_MED       0x2721
#define SHT3X_CMD_PERI_10_LOW       0x272A

/* Advanced Features & System Commands */
#define SHT3X_CMD_FETCH_DATA        0xE000
#define SHT3X_CMD_ART               0x2B32
#define SHT3X_CMD_BREAK             0x3093
#define SHT3X_CMD_CLEAR_STATUS      0x3041
#define SHT3X_CMD_READ_STATUS       0xF32D
#define SHT3X_CMD_HEATER_ENABLE     0x306D
#define SHT3X_CMD_HEATER_DISABLE    0x3066
#define SHT3X_CMD_SOFT_RESET        0x30A2

/* --- TIMING & CRC CONSTANTS --- */
#define SHT3X_CRC_POLYNOMIAL        0x31
#define SHT3X_CRC_INIT              0xFF
#define SHT3X_CONV_TIME_LOW_MS      5
#define SHT3X_CONV_TIME_MED_MS      7
#define SHT3X_CONV_TIME_HIGH_MS     16

/* --- DATA TYPES --- */
typedef enum {
    SHT3X_OK    = 0,
    SHT3X_ERROR = -1
} SHT3x_Status_t;

typedef enum {
    SHT3X_REPEAT_LOW    = 0,
    SHT3X_REPEAT_MEDIUM = 1,
    SHT3X_REPEAT_HIGH   = 2
} SHT3x_Repeatability_t;

typedef enum {
    SHT3X_CS_DISABLED   = 0,
    SHT3X_CS_ENABLED    = 1
} SHT3x_ClockStretching_t;

typedef enum {
    SHT3X_MPS_0_5       = 0,
    SHT3X_MPS_1         = 1,
    SHT3X_MPS_2         = 2,
    SHT3X_MPS_4         = 4,
    SHT3X_MPS_10        = 10
} SHT3x_MPS_t;

typedef struct {
    float temperature; /* Celsius (°C) */
    float humidity;    /* Relative Humidity (%RH) */
} SHT3x_Data_t;

/* --- FUNCTION PROTOTYPES --- */

/* System Management */
SHT3x_Status_t SHT3x_Init(uint8_t address, uint32_t timeout);
SHT3x_Status_t SHT3x_SoftReset(uint8_t address, uint32_t timeout);
SHT3x_Status_t SHT3x_SetHeater(uint8_t address, uint8_t enable, uint32_t timeout);
SHT3x_Status_t SHT3x_ReadStatusRegister(uint8_t address, uint16_t* status_reg, uint32_t timeout);

/* Single Shot Measurement */
SHT3x_Status_t SHT3x_ReadSingleShot(uint8_t address, SHT3x_Repeatability_t repeat,
                                    SHT3x_ClockStretching_t cs, SHT3x_Data_t* out_data, uint32_t timeout);

/* Periodic Measurement */
SHT3x_Status_t SHT3x_StartPeriodicMeas(uint8_t address, SHT3x_Repeatability_t repeat, SHT3x_MPS_t mps, uint32_t timeout);
SHT3x_Status_t SHT3x_FetchPeriodicData(uint8_t address, SHT3x_Data_t* out_data, uint32_t timeout);
SHT3x_Status_t SHT3x_StopPeriodicMeas(uint8_t address, uint32_t timeout);

/* Accelerated Response Time (ART) Measurement */
SHT3x_Status_t SHT3x_StartARTMeas(uint8_t address, uint32_t timeout);

#endif /* SHT3X_H_ */
