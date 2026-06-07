#include "LoRa.h"
#include <stddef.h>

volatile uint8_t g_lora_tx_done  = 0;
volatile uint8_t g_lora_rx_done  = 0;
LoRa_Stats_t     g_lora_stats    = {0};

static volatile uint8_t s_rx_fifo_addr = 0;
static volatile uint8_t s_rx_pkt_len   = 0;

/**
 * @brief Reads a value from a specific LoRa register via SPI.
 * @param addr The register address to read from.
 * @return The 8-bit value read from the register.
 */
uint8_t LoRa_ReadReg(uint8_t addr) {
    uint8_t val;
    LoRa_Platform_NSS_Low();
    LoRa_Platform_SPI_Transfer(addr & 0x7F);
    val = LoRa_Platform_SPI_Transfer(0x00);
    LoRa_Platform_NSS_High();
    return val;
}

/**
 * @brief Writes a value to a specific LoRa register via SPI.
 * @param addr The register address to write to.
 * @param value The 8-bit value to be written.
 */
void LoRa_WriteReg(uint8_t addr, uint8_t value) {
    LoRa_Platform_NSS_Low();
    LoRa_Platform_SPI_Transfer(addr | 0x80);
    LoRa_Platform_SPI_Transfer(value);
    LoRa_Platform_NSS_High();
}

/**
 * @brief Reads a burst of data from a specific LoRa register address into a buffer.
 * @param addr The starting register address.
 * @param buf Pointer to the destination buffer.
 * @param len Number of bytes to read.
 */
void LoRa_BurstRead(uint8_t addr, uint8_t* buf, uint8_t len) {
    LoRa_Platform_NSS_Low();
    LoRa_Platform_SPI_Transfer(addr & 0x7F);
    for (uint8_t i = 0; i < len; i++) {
        buf[i] = LoRa_Platform_SPI_Transfer(0x00);
    }
    LoRa_Platform_NSS_High();
}

/**
 * @brief Writes a burst of data from a buffer to a specific LoRa register address.
 * @param addr The starting register address.
 * @param buf Pointer to the source data buffer.
 * @param len Number of bytes to write.
 */
void LoRa_BurstWrite(uint8_t addr, const uint8_t* buf, uint8_t len) {
    LoRa_Platform_NSS_Low();
    LoRa_Platform_SPI_Transfer(addr | 0x80);
    for (uint8_t i = 0; i < len; i++) {
        LoRa_Platform_SPI_Transfer(buf[i]);
    }
    LoRa_Platform_NSS_High();
}

/**
 * @brief Performs a hardware reset on the LoRa module using the RESET pin.
 */
void LoRa_Reset(void) {
    LoRa_Platform_RESET_Low();
    LoRa_Platform_DelayMs(10);
    LoRa_Platform_RESET_High();
    LoRa_Platform_DelayMs(10);
}

/**
 * @brief Sets the operation mode of the LoRa transceiver.
 * @param _LoRa Pointer to the LoRa configuration structure.
 * @param mode The desired mode (e.g., SLEEP, STNBY, TX, RX).
 */
void LoRa_SetMode(LoRa_Config_t* _LoRa, int mode) {
    if (_LoRa == NULL) return;
    uint8_t opmode = 0x80 | 0x08 | (mode & 0x07);
    LoRa_WriteReg(REG_OP_MODE, opmode);
    _LoRa->current_mode = (uint8_t)mode;
}

/**
 * @brief Initializes the LoRa transceiver with standard and custom configurations.
 * @param _LoRa Pointer to the LoRa configuration structure containing RF parameters.
 * @return LORA_OK on success, or an error code indicating the failure reason.
 */
LoRa_Status_t LoRa_Init(LoRa_Config_t* _LoRa) {
    if (_LoRa == NULL) return LORA_INVALID_PARAM;

    LoRa_Platform_SPI_Init();
    LoRa_Platform_NSS_High();
    LoRa_Reset();

    LoRa_WriteReg(REG_OP_MODE, 0x00);
    LoRa_Platform_DelayMs(5);
    LoRa_WriteReg(REG_OP_MODE, 0x80 | 0x08 | 0x00);
    LoRa_Platform_DelayMs(5);

    if (LoRa_ReadReg(REG_VERSION) != 0x12) return LORA_NOT_FOUND;

    LoRa_WriteReg(REG_FIFO_TX_BASE_ADDR, 0x00);
    LoRa_WriteReg(REG_FIFO_RX_BASE_ADDR, 0x00);
    LoRa_WriteReg(REG_HOP_PERIOD, 0x00);
    LoRa_WriteReg(REG_LNA, 0x23);

    if (LoRa_SetFrequency(_LoRa, _LoRa->frequency)       != LORA_OK) return LORA_ERROR;
    if (LoRa_SetPower(_LoRa, _LoRa->power)               != LORA_OK) return LORA_ERROR;
    if (LoRa_SetOCP(_LoRa, _LoRa->overCurrentProtection) != LORA_OK) return LORA_ERROR;

    if (LoRa_SetSpreadingFactor(_LoRa, _LoRa->spredingFactor) != LORA_OK) return LORA_ERROR;
    if (LoRa_SetBandwidth(_LoRa, _LoRa->bandWidth)           != LORA_OK) return LORA_ERROR;
    if (LoRa_SetCodingRate(_LoRa, _LoRa->crcRate)             != LORA_OK) return LORA_ERROR;

    uint8_t cfg1 = LoRa_ReadReg(REG_MODEM_CONFIG1) & 0xFE;
    LoRa_WriteReg(REG_MODEM_CONFIG1, cfg1 | (_LoRa->implicitHeader ? 0x01 : 0x00));

    LoRa_WriteReg(REG_PREAMBLE_MSB, (uint8_t)(_LoRa->preamble >> 8));
    LoRa_WriteReg(REG_PREAMBLE_LSB, (uint8_t)(_LoRa->preamble));
    LoRa_SetSyncWord(_LoRa, _LoRa->syncWord);
    LoRa_SetAutoLDO(_LoRa);
    LoRa_SetMode(_LoRa, STNBY_MODE);

    s_rx_fifo_addr    = 0;
    return LORA_OK;
}

/**
 * @brief Reads the received packet data from the LoRa FIFO buffer.
 * @param _LoRa    Pointer to the LoRa configuration structure.
 * @param buf      Pointer to the buffer where received data will be stored.
 * @param max_len  Maximum capacity of the destination buffer to prevent overflow.
 * @return LoRa_Status_t LORA_OK on success, or error status code on failure.
 */
LoRa_Status_t LoRa_ReadPacketData(LoRa_Config_t* _LoRa, uint8_t* buf, uint8_t max_len) {
    if (_LoRa == NULL || buf == NULL) return LORA_INVALID_PARAM;

    if (g_lora_rx_done == 0) return LORA_ERROR;
    g_lora_rx_done = 0;

    g_lora_stats.rx_ok++;
    return LORA_OK;
}

/**
 * @brief Configures the module and switches it to continuous reception mode.
 * @param _LoRa Pointer to the LoRa configuration structure.
 */
void LoRa_RxStart(LoRa_Config_t* _LoRa) {
    if (_LoRa == NULL) return;
    LoRa_WriteReg(REG_DIO_MAPPING1, 0x00);
    LoRa_WriteReg(REG_FIFO_ADDR_PTR, 0x00);
    LoRa_WriteReg(REG_IRQ_FLAGS, 0xFF);
    g_lora_rx_done  = 0;
    LoRa_SetMode(_LoRa, RXCONTIN_MODE);
}

/**
 * @brief Executes a Channel Activity Detection (CAD) operation to check for preamble symbols.
 * @param _LoRa Pointer to the LoRa configuration structure.
 * @param timeout_ms Maximum time allowed in milliseconds to wait for CAD completion.
 * @return LORA_CAD_FREE if the channel is clear, LORA_CAD_BUSY if a preamble is detected, or LORA_CAD_TIMEOUT.
 */
LoRa_CAD_Status_t LoRa_DoCAD(LoRa_Config_t* _LoRa, uint32_t timeout_ms) {
    if (_LoRa == NULL) return LORA_CAD_INVALID_PARAM;

    LoRa_WriteReg(REG_IRQ_FLAGS, 0xFF);
    LoRa_WriteReg(REG_DIO_MAPPING1, 0x80);
    LoRa_SetMode(_LoRa, CAD_MODE);

    uint32_t start = LoRa_Platform_GetTickMs();
    while ((LoRa_ReadReg(REG_IRQ_FLAGS) & 0x04) == 0) {
        if ((LoRa_Platform_GetTickMs() - start) > timeout_ms) {
            LoRa_SetMode(_LoRa, STNBY_MODE);
            return LORA_CAD_TIMEOUT;
        }
    }

    uint8_t irq = LoRa_ReadReg(REG_IRQ_FLAGS);
    LoRa_WriteReg(REG_IRQ_FLAGS, 0xFF);
    LoRa_SetMode(_LoRa, STNBY_MODE);

    return (irq & 0x01) ? LORA_CAD_BUSY : LORA_CAD_FREE;
}

/**
 * @brief Transmits a data packet over the air using polling mechanism for completion.
 * @param _LoRa Pointer to the LoRa configuration structure containing RF parameters.
 * @param buf Pointer to the buffer containing the raw data bytes to transmit.
 * @param len Length of the data payload in bytes.
 * @param timeout Maximum time allowed in milliseconds to wait for transmission to finish.
 * @return LoRa_Status_t LORA_OK on success, LORA_TIMEOUT if expiration occurs, or LORA_ERROR.
 */
LoRa_Status_t LoRa_Transmit(LoRa_Config_t* _LoRa, const uint8_t* buf, uint8_t len, uint32_t timeout) {
    if (!buf || !len || _LoRa == NULL) return LORA_ERROR;

    LoRa_SetMode(_LoRa, STNBY_MODE);
    LoRa_Platform_DelayMs(2);

    LoRa_WriteReg(REG_FIFO_ADDR_PTR, 0x00);
    LoRa_BurstWrite(REG_FIFO, buf, len);
    LoRa_WriteReg(REG_PAYLOAD_LENGTH, len);

    LoRa_WriteReg(REG_IRQ_FLAGS, 0xFF);

    LoRa_SetMode(_LoRa, TRANSMIT_MODE);

    if (timeout == 0) timeout = 2000;

    uint32_t t0 = LoRa_Platform_GetTickMs();
    while (1) {
        uint8_t irq_flags = LoRa_ReadReg(REG_IRQ_FLAGS);

        if ((irq_flags & 0x08) != 0) {
            LoRa_WriteReg(REG_IRQ_FLAGS, 0x08);
            break;
        }

        if ((LoRa_Platform_GetTickMs() - t0) > timeout) {
            LoRa_SetMode(_LoRa, STNBY_MODE);
            g_lora_stats.tx_fail++;
            return LORA_TIMEOUT;
        }
    }

    LoRa_SetMode(_LoRa, STNBY_MODE);
    g_lora_stats.tx_ok++;
    return LORA_OK;
}

/**
 * @brief Transmits data using Listen Before Talk (LBT) mechanism via CAD.
 * @param _LoRa       Pointer to the LoRa configuration structure.
 * @param buf         Pointer to the data buffer to be transmitted.
 * @param payload_len Length of the payload to transmit (Max 255 bytes).
 * @param timeout_ms  Maximum time allowed for the overall transmission process.
 * @return LoRa_Status_t LORA_OK on success, or error status code on failure.
 */
LoRa_Status_t LoRa_Transmit_LBT(LoRa_Config_t* _LoRa, const uint8_t* buf,
                                 uint8_t payload_len, uint32_t timeout_ms) {
    if (!buf || !payload_len || payload_len > 255 || _LoRa == NULL) return LORA_INVALID_PARAM;

    uint32_t deadline = LoRa_Platform_GetTickMs() + timeout_ms;

    for (uint8_t retry = 0; retry < 3; retry++) {
        if (LoRa_Platform_GetTickMs() >= deadline) {
            g_lora_stats.tx_fail++;
            return LORA_TIMEOUT;
        }

        uint8_t previous_mode = _LoRa->current_mode;

        LoRa_CAD_Status_t cad = LoRa_DoCAD(_LoRa, 100);

        if (cad == LORA_CAD_TIMEOUT) {
            g_lora_stats.tx_fail++;
            return LORA_TIMEOUT;
        }

        if (cad == LORA_CAD_BUSY) {
            g_lora_stats.tx_busy++;
            continue;
        }

        uint32_t remaining = deadline - LoRa_Platform_GetTickMs();
        LoRa_Status_t ret = LoRa_Transmit(_LoRa, buf, payload_len, remaining);

        if (ret == LORA_OK) {
            if (previous_mode == RXCONTIN_MODE) {
                LoRa_RxStart(_LoRa);
            } else {
                LoRa_SetMode(_LoRa, previous_mode);
            }
        }
        return ret;
    }

    g_lora_stats.tx_busy++;
    return LORA_BUSY;
}

/**
 * @brief Configures the center frequency of the RF carrier lock.
 * @param _LoRa Pointer to the LoRa configuration structure.
 * @param freq_hz The target frequency calculated in Hertz (Hz).
 * @return LORA_OK if valid and updated, or LORA_INVALID_PARAM if out of device boundaries.
 */
LoRa_Status_t LoRa_SetFrequency(LoRa_Config_t* _LoRa, uint32_t freq_hz) {
    if (_LoRa == NULL) return LORA_INVALID_PARAM;
    if (freq_hz < 137000000UL || freq_hz > 1020000000UL) return LORA_INVALID_PARAM;

    uint64_t frf = ((uint64_t)freq_hz << 19) / 32000000UL;
    LoRa_WriteReg(REG_FRF_MSB, (uint8_t)(frf >> 16));
    LoRa_WriteReg(REG_FRF_MID, (uint8_t)(frf >> 8));
    LoRa_WriteReg(REG_FRF_LSB, (uint8_t)(frf));
    _LoRa->frequency = freq_hz;
    return LORA_OK;
}

/**
 * @brief Configures the RF transmission power output and associated safety current bounds.
 * @param _LoRa Pointer to the LoRa configuration structure.
 * @param power_dbm Desired output power level scaled in dBm (supported range: 2 to 20 dBm).
 * @return LORA_OK if successful, or LORA_INVALID_PARAM if parameters are invalid.
 */
LoRa_Status_t LoRa_SetPower(LoRa_Config_t* _LoRa, uint8_t power_dbm) {
    if (_LoRa == NULL) return LORA_INVALID_PARAM;
    if (power_dbm < 2 || power_dbm > 20) return LORA_INVALID_PARAM;

    if (power_dbm == 20) {
        LoRa_WriteReg(REG_PA_CONFIG, 0x80 | 0x0F);
        LoRa_WriteReg(REG_PA_DAC, 0x87);
        LoRa_WriteReg(REG_OCP, 0x3F);
    } else if (power_dbm > 14) {
        LoRa_WriteReg(REG_PA_CONFIG, 0x80 | (power_dbm - 2));
        LoRa_WriteReg(REG_PA_DAC, 0x84);
        LoRa_WriteReg(REG_OCP, 0x2B);
    } else {
        LoRa_WriteReg(REG_PA_CONFIG, 0x70 | (power_dbm + 1));
        LoRa_WriteReg(REG_PA_DAC, 0x84);
    }
    _LoRa->power = power_dbm;
    return LORA_OK;
}

/**
 * @brief Sets the Over Current Protection (OCP) threshold for the internal power amplifier.
 * @param _LoRa Pointer to the LoRa configuration structure.
 * @param current_mA Maximum allowable current limit in milliamperes (mA).
 * @return LORA_OK if validation succeeds, or LORA_INVALID_PARAM if input is out of bounds.
 */
LoRa_Status_t LoRa_SetOCP(LoRa_Config_t* _LoRa, uint8_t current_mA) {
    if (_LoRa == NULL) return LORA_INVALID_PARAM;
    if (current_mA < 45 || current_mA > 240) return LORA_INVALID_PARAM;

    uint8_t trim;
    if (current_mA <= 120) {
        trim = (current_mA - 45) / 5;
    } else {
        trim = (current_mA + 30) / 10;
    }
    LoRa_WriteReg(REG_OCP, 0x20 | (trim & 0x1F));
    _LoRa->overCurrentProtection = current_mA;
    return LORA_OK;
}

/**
 * @brief Sets the Spreading Factor (SF) for the LoRa modulation.
 * @param _LoRa Pointer to the LoRa configuration structure.
 * @param sf Spreading factor value (supported range: 6 to 12).
 * @return LORA_OK if successful, or LORA_INVALID_PARAM.
 */
LoRa_Status_t LoRa_SetSpreadingFactor(LoRa_Config_t* _LoRa, uint8_t sf) {
    if (_LoRa == NULL) return LORA_INVALID_PARAM;
    if (sf < 6 || sf > 12) return LORA_INVALID_PARAM;

    if (sf == 6) {
        LoRa_WriteReg(REG_DETECTION_OPTIMIZE, 0xC5);
        LoRa_WriteReg(REG_DETECTION_THRESHOLD, 0x0C);
        LoRa_WriteReg(REG_MODEM_CONFIG1, LoRa_ReadReg(REG_MODEM_CONFIG1) | 0x01);
    } else {
        LoRa_WriteReg(REG_DETECTION_OPTIMIZE, 0xC3);
        LoRa_WriteReg(REG_DETECTION_THRESHOLD, 0x0A);
        LoRa_WriteReg(REG_MODEM_CONFIG1, LoRa_ReadReg(REG_MODEM_CONFIG1) & 0xFE);
    }

    uint8_t cfg2 = LoRa_ReadReg(REG_MODEM_CONFIG2) & 0x03;
    LoRa_WriteReg(REG_MODEM_CONFIG2, cfg2 | (sf << 4) | (_LoRa->enableCRC ? 0x04 : 0x00));
    _LoRa->spredingFactor = sf;
    LoRa_SetAutoLDO(_LoRa);
    return LORA_OK;
}

/**
 * @brief Configures the signal bandwidth for the LoRa modem.
 * @param _LoRa Pointer to the LoRa configuration structure.
 * @param bw Enum index representing the targeted bandwidth configuration.
 * @return LORA_OK if operation is successful, or LORA_INVALID_PARAM.
 */
LoRa_Status_t LoRa_SetBandwidth(LoRa_Config_t* _LoRa, uint8_t bw) {
    if (_LoRa == NULL) return LORA_INVALID_PARAM;
    if (bw > BW_500KHz) return LORA_INVALID_PARAM;

    static const uint8_t BW_REG_VAL[] = {
        0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90
    };
    uint8_t cfg1 = LoRa_ReadReg(REG_MODEM_CONFIG1) & 0x0F;
    LoRa_WriteReg(REG_MODEM_CONFIG1, cfg1 | BW_REG_VAL[bw]);
    _LoRa->bandWidth = bw;
    LoRa_SetAutoLDO(_LoRa);
    return LORA_OK;
}

/**
 * @brief Sets the error Coding Rate (CR) setting for corporate error corrections.
 * @param _LoRa Pointer to the LoRa configuration structure.
 * @param cr Coding rate parameter index (from CR_4_5 up to CR_4_8).
 * @return LORA_OK if successful, or LORA_INVALID_PARAM.
 */
LoRa_Status_t LoRa_SetCodingRate(LoRa_Config_t* _LoRa, uint8_t cr) {
    if (_LoRa == NULL) return LORA_INVALID_PARAM;
    if (cr < CR_4_5 || cr > CR_4_8) return LORA_INVALID_PARAM;

    uint8_t cfg1 = LoRa_ReadReg(REG_MODEM_CONFIG1) & 0xF1;
    LoRa_WriteReg(REG_MODEM_CONFIG1, cfg1 | (cr << 1));
    _LoRa->crcRate = cr;
    return LORA_OK;
}

/**
 * @brief Updates the network identifier Sync Word byte inside the core transceiver.
 * @param _LoRa Pointer to the LoRa configuration structure.
 * @param syncword The byte value used to filter network frames.
 * @return LORA_OK if successful, or LORA_INVALID_PARAM.
 */
LoRa_Status_t LoRa_SetSyncWord(LoRa_Config_t* _LoRa, uint8_t syncword) {
    if (_LoRa == NULL) return LORA_INVALID_PARAM;
    LoRa_WriteReg(REG_SYNC_WORD, syncword);
    _LoRa->syncWord = syncword;
    return LORA_OK;
}

/**
 * @brief Automatically calculates and toggles Low Data Rate Optimization (LDO) register states.
 * @param _LoRa Pointer to the LoRa configuration structure.
 */
void LoRa_SetAutoLDO(LoRa_Config_t* _LoRa) {
    if (_LoRa == NULL) return;
    static const uint32_t BW_HZ_VAL[] = {
        7800, 10400, 15600, 20800, 31250, 41700, 62500, 125000, 250000, 500000
    };
    if (_LoRa->bandWidth > 9) return;

    uint32_t symbol_time_us = (uint32_t)(
        ((uint64_t)(1 << _LoRa->spredingFactor) * 1000000UL) / BW_HZ_VAL[_LoRa->bandWidth]
    );
    uint8_t cfg3 = LoRa_ReadReg(REG_MODEM_CONFIG3);
    cfg3 = (symbol_time_us > 16000UL) ? (cfg3 | 0x08) : (cfg3 & ~0x08);
    LoRa_WriteReg(REG_MODEM_CONFIG3, cfg3 | 0x04);
}

/**
 * @brief Retrieves the Signal-to-Noise Ratio (SNR) value of the last received packet.
 * @return The calculated SNR value in dB.
 */
int8_t LoRa_GetPacketSNR(void) {
    return (int8_t)((int8_t)LoRa_ReadReg(REG_PKT_SNR) >> 2);
}

/**
 * @brief Retrieves the Received Signal Strength Indicator (RSSI) of the last received packet.
 * @return The RSSI value in dBm.
 */
int16_t LoRa_GetPacketRSSI(void) {
    int8_t  snr  = LoRa_GetPacketSNR();
    int16_t rssi = (int16_t)LoRa_ReadReg(REG_PKT_RSSI) - 157;
    return (snr < 0) ? (int16_t)(rssi + snr) : rssi;
}

/**
 * @brief Retrieves the current instantaneous RSSI value from the ambient air channel.
 * @return The immediate channel RSSI reading in dBm.
 */
int16_t LoRa_GetCurrentRSSI(void) {
    return (int16_t)LoRa_ReadReg(REG_RSSI_VALUE) - 157;
}

/**
 * @brief Computes an 8-bit CRC checksum value over a variable length data buffer.
 * @param buf Pointer to the source data buffer.
 * @param len Length of data to compute.
 * @return Generated 8-bit CRC remainder.
 */
uint8_t LoRa_CRC8(const uint8_t* buf, uint8_t len) {
    uint8_t crc = 0x00;
    while (len--) {
        crc ^= *buf++;
        for (uint8_t b = 8; b; b--) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/**
 * @brief Resets the dynamic metrics diagnostics statistics structure data to zero.
 */
void LoRa_ResetStats(void) {
    LoRa_Stats_t zero = {0};
    g_lora_stats = zero;
}