/**
 ******************************************************************************
 * @file    sht3x.c
 * @brief   Full implementation of SHT3x-DIS sensor driver.
 ******************************************************************************
 */
#include "sht3x.h"
#include "platform.h" /* Dependency on Platform abstraction layer */

/* ========================================================================== */
/* PRIVATE UTILITY FUNCTIONS                                                  */
/* ========================================================================== */

static uint8_t SHT3x_CheckCrc8(uint8_t *data, uint8_t nBytes) {
    uint8_t crc = SHT3X_CRC_INIT;
    for (uint8_t byteIndex = 0; byteIndex < nBytes; byteIndex++) {
        crc ^= data[byteIndex];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ SHT3X_CRC_POLYNOMIAL;
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}

static SHT3x_Status_t SHT3x_WriteCommand(uint8_t address, uint16_t command, uint32_t timeout) {
    uint8_t write_addr = address & 0xFE;
    uint8_t cmd_buffer[2];

    cmd_buffer[0] = (uint8_t)((command >> 8) & 0xFF);
    cmd_buffer[1] = (uint8_t)(command & 0xFF);

    if (SHT3x_Platform_I2C_Transmit(write_addr, cmd_buffer, 2, timeout) == 0) {
        return SHT3X_OK;
    }
    return SHT3X_ERROR;
}

static void SHT3x_CalculateValues(uint8_t* raw_buf, SHT3x_Data_t* out_data) {
    uint16_t raw_temp = (uint16_t)((raw_buf[0] << 8) | raw_buf[1]);
    uint16_t raw_humi = (uint16_t)((raw_buf[3] << 8) | raw_buf[4]);

    out_data->temperature = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
    out_data->humidity    = 100.0f * ((float)raw_humi / 65535.0f);

    if (out_data->humidity > 100.0f) out_data->humidity = 100.0f;
    if (out_data->humidity < 0.0f)   out_data->humidity = 0.0f;
}

/* ========================================================================== */
/* SYSTEM MANAGEMENT APIs                                                     */
/* ========================================================================== */

SHT3x_Status_t SHT3x_Init(uint8_t address, uint32_t timeout) {
    SHT3x_Platform_Delay_ms(2); /* Power-up delay */
    uint16_t status_reg = 0;

    /* Ping device by reading status register */
    if (SHT3x_ReadStatusRegister(address, &status_reg, timeout) != SHT3X_OK) {
        return SHT3X_ERROR;
    }
    return SHT3x_WriteCommand(address, SHT3X_CMD_CLEAR_STATUS, timeout);
}

SHT3x_Status_t SHT3x_SoftReset(uint8_t address, uint32_t timeout) {
    if (SHT3x_WriteCommand(address, SHT3X_CMD_SOFT_RESET, timeout) != SHT3X_OK) {
        return SHT3X_ERROR;
    }
    SHT3x_Platform_Delay_ms(2); /* Soft reset execution time */
    return SHT3X_OK;
}

SHT3x_Status_t SHT3x_SetHeater(uint8_t address, uint8_t enable, uint32_t timeout) {
    uint16_t cmd = (enable) ? SHT3X_CMD_HEATER_ENABLE : SHT3X_CMD_HEATER_DISABLE;
    return SHT3x_WriteCommand(address, cmd, timeout);
}

SHT3x_Status_t SHT3x_ReadStatusRegister(uint8_t address, uint16_t* status_reg, uint32_t timeout) {
    uint8_t buffer[3];
    uint8_t read_addr = address | 0x01;

    if (SHT3x_WriteCommand(address, SHT3X_CMD_READ_STATUS, timeout) != SHT3X_OK) return SHT3X_ERROR;
    if (SHT3x_Platform_I2C_Receive(read_addr, buffer, 3, timeout) != 0) return SHT3X_ERROR;
    if (SHT3x_CheckCrc8(buffer, 2) != buffer[2]) return SHT3X_ERROR;

    *status_reg = (uint16_t)((buffer[0] << 8) | buffer[1]);
    return SHT3X_OK;
}

/* ========================================================================== */
/* SINGLE SHOT MEASUREMENT MODE                                               */
/* ========================================================================== */

SHT3x_Status_t SHT3x_ReadSingleShot(uint8_t address, SHT3x_Repeatability_t repeat,
                                    SHT3x_ClockStretching_t cs, SHT3x_Data_t* out_data, uint32_t timeout)
{
    uint16_t cmd = 0;
    uint32_t delay_ms = 0;
    uint8_t raw_buf[6];

    /* Map parameters to command matrix */
    if (cs == SHT3X_CS_ENABLED) {
        if      (repeat == SHT3X_REPEAT_LOW)    { cmd = SHT3X_CMD_MEAS_CS_LOW;  delay_ms = SHT3X_CONV_TIME_LOW_MS; }
        else if (repeat == SHT3X_REPEAT_MEDIUM) { cmd = SHT3X_CMD_MEAS_CS_MED;  delay_ms = SHT3X_CONV_TIME_MED_MS; }
        else                                    { cmd = SHT3X_CMD_MEAS_CS_HIGH; delay_ms = SHT3X_CONV_TIME_HIGH_MS; }
    } else {
        if      (repeat == SHT3X_REPEAT_LOW)    { cmd = SHT3X_CMD_MEAS_LOW;     delay_ms = SHT3X_CONV_TIME_LOW_MS; }
        else if (repeat == SHT3X_REPEAT_MEDIUM) { cmd = SHT3X_CMD_MEAS_MED;     delay_ms = SHT3X_CONV_TIME_MED_MS; }
        else                                    { cmd = SHT3X_CMD_MEAS_HIGH;    delay_ms = SHT3X_CONV_TIME_HIGH_MS; }
    }

    if (SHT3x_WriteCommand(address, cmd, timeout) != SHT3X_OK) return SHT3X_ERROR;

    if (cs == SHT3X_CS_DISABLED) {
        SHT3x_Platform_Delay_ms(delay_ms);
    }

    uint8_t read_addr = address | 0x01;
    if (SHT3x_Platform_I2C_Receive(read_addr, raw_buf, 6, timeout) != 0) return SHT3X_ERROR;

    if (SHT3x_CheckCrc8(&raw_buf[0], 2) != raw_buf[2]) return SHT3X_ERROR;
    if (SHT3x_CheckCrc8(&raw_buf[3], 2) != raw_buf[5]) return SHT3X_ERROR;

    SHT3x_CalculateValues(raw_buf, out_data);
    return SHT3X_OK;
}

/* ========================================================================== */
/* PERIODIC MEASUREMENT MODE                                                  */
/* ========================================================================== */

SHT3x_Status_t SHT3x_StartPeriodicMeas(uint8_t address, SHT3x_Repeatability_t repeat, SHT3x_MPS_t mps, uint32_t timeout) {
    uint16_t cmd = 0;

    switch (mps) {
        case SHT3X_MPS_0_5:
            cmd = (repeat == SHT3X_REPEAT_LOW) ? SHT3X_CMD_PERI_0_5_LOW :
                  (repeat == SHT3X_REPEAT_MEDIUM) ? SHT3X_CMD_PERI_0_5_MED : SHT3X_CMD_PERI_0_5_HIGH; break;
        case SHT3X_MPS_1:
            cmd = (repeat == SHT3X_REPEAT_LOW) ? SHT3X_CMD_PERI_1_LOW :
                  (repeat == SHT3X_REPEAT_MEDIUM) ? SHT3X_CMD_PERI_1_MED : SHT3X_CMD_PERI_1_HIGH; break;
        case SHT3X_MPS_2:
            cmd = (repeat == SHT3X_REPEAT_LOW) ? SHT3X_CMD_PERI_2_LOW :
                  (repeat == SHT3X_REPEAT_MEDIUM) ? SHT3X_CMD_PERI_2_MED : SHT3X_CMD_PERI_2_HIGH; break;
        case SHT3X_MPS_4:
            cmd = (repeat == SHT3X_REPEAT_LOW) ? SHT3X_CMD_PERI_4_LOW :
                  (repeat == SHT3X_REPEAT_MEDIUM) ? SHT3X_CMD_PERI_4_MED : SHT3X_CMD_PERI_4_HIGH; break;
        case SHT3X_MPS_10:
            cmd = (repeat == SHT3X_REPEAT_LOW) ? SHT3X_CMD_PERI_10_LOW :
                  (repeat == SHT3X_REPEAT_MEDIUM) ? SHT3X_CMD_PERI_10_MED : SHT3X_CMD_PERI_10_HIGH; break;
        default: return SHT3X_ERROR;
    }

    return SHT3x_WriteCommand(address, cmd, timeout);
}

SHT3x_Status_t SHT3x_FetchPeriodicData(uint8_t address, SHT3x_Data_t* out_data, uint32_t timeout) {
    uint8_t raw_buf[6];
    uint8_t read_addr = address | 0x01;

    if (SHT3x_WriteCommand(address, SHT3X_CMD_FETCH_DATA, timeout) != SHT3X_OK) return SHT3X_ERROR;
    if (SHT3x_Platform_I2C_Receive(read_addr, raw_buf, 6, timeout) != 0) return SHT3X_ERROR;

    if (SHT3x_CheckCrc8(&raw_buf[0], 2) != raw_buf[2]) return SHT3X_ERROR;
    if (SHT3x_CheckCrc8(&raw_buf[3], 2) != raw_buf[5]) return SHT3X_ERROR;

    SHT3x_CalculateValues(raw_buf, out_data);
    return SHT3X_OK;
}

SHT3x_Status_t SHT3x_StopPeriodicMeas(uint8_t address, uint32_t timeout) {
    if (SHT3x_WriteCommand(address, SHT3X_CMD_BREAK, timeout) != SHT3X_OK) return SHT3X_ERROR;
    SHT3x_Platform_Delay_ms(1); /* Wait for sensor to enter Idle state */
    return SHT3X_OK;
}

/* ========================================================================== */
/* ADVANCED ART (ACCELERATED RESPONSE TIME) MODE                              */
/* ========================================================================== */

SHT3x_Status_t SHT3x_StartARTMeas(uint8_t address, uint32_t timeout) {
    /* Starts periodic measurements at 4Hz */
    return SHT3x_WriteCommand(address, SHT3X_CMD_ART, timeout);
}
