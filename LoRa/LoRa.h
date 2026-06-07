#ifndef LORA_H
#define LORA_H

#include <stdint.h>

#define REG_FIFO                 0x00
#define REG_OP_MODE              0x01
#define REG_FRF_MSB              0x06
#define REG_FRF_MID              0x07
#define REG_FRF_LSB              0x08
#define REG_PA_CONFIG            0x09
#define REG_OCP                  0x0B
#define REG_LNA                  0x0C
#define REG_FIFO_ADDR_PTR        0x0D
#define REG_FIFO_TX_BASE_ADDR    0x0E
#define REG_FIFO_RX_BASE_ADDR    0x0F
#define REG_FIFO_RX_CURRENT      0x10
#define REG_IRQ_FLAGS            0x12
#define REG_RX_NB_BYTES          0x13
#define REG_PKT_SNR              0x19
#define REG_PKT_RSSI             0x1A
#define REG_RSSI_VALUE           0x1B
#define REG_MODEM_CONFIG1        0x1D
#define REG_MODEM_CONFIG2        0x1E
#define REG_SYMB_TIMEOUT_LSB     0x1F
#define REG_PREAMBLE_MSB         0x20
#define REG_PREAMBLE_LSB         0x21
#define REG_PAYLOAD_LENGTH       0x22
#define REG_MODEM_CONFIG3        0x26
#define REG_HOP_PERIOD           0x24
#define REG_DETECTION_OPTIMIZE   0x31
#define REG_DETECTION_THRESHOLD  0x37
#define REG_SYNC_WORD            0x39
#define REG_DIO_MAPPING1         0x40
#define REG_VERSION              0x42
#define REG_PA_DAC               0x4D

#define SLEEP_MODE               0x00
#define STNBY_MODE               0x01
#define TRANSMIT_MODE            0x03
#define RXCONTIN_MODE            0x05
#define RXSINGLE_MODE            0x06
#define CAD_MODE                 0x07

#define BW_7_8KHz                0
#define BW_10_4KHz               1
#define BW_15_6KHz               2
#define BW_20_8KHz               3
#define BW_31_25KHz              4
#define BW_41_7KHz               5
#define BW_62_5KHz               6
#define BW_125KHz                7
#define BW_250KHz                8
#define BW_500KHz                9

#define CR_4_5                   1
#define CR_4_6                   2
#define CR_4_7                   3
#define CR_4_8                   4

typedef enum {
    LORA_OK             = 0,
    LORA_ERROR          = 1,
    LORA_INVALID_PARAM  = 2,
    LORA_NOT_FOUND      = 3,
    LORA_TIMEOUT        = 4,
    LORA_BUSY           = 5
} LoRa_Status_t;

typedef enum {
    LORA_CAD_FREE            = 0,
    LORA_CAD_BUSY            = 1,
    LORA_CAD_TIMEOUT         = 2,
    LORA_CAD_INVALID_PARAM   = 3
} LoRa_CAD_Status_t;

typedef struct {
    uint32_t          frequency;
    uint16_t          preamble;
    uint8_t           spredingFactor;
    uint8_t           bandWidth;
    uint8_t           crcRate;
    uint8_t           power;
    uint8_t           overCurrentProtection;
    uint8_t           syncWord;
    uint8_t           nodeId;
    uint8_t           current_mode;
    uint8_t           enableCRC;
    uint8_t           implicitHeader;
} LoRa_Config_t;

typedef struct {
    uint32_t tx_ok;
    uint32_t tx_fail;
    uint32_t tx_busy;
    uint32_t rx_ok;
    uint32_t rx_crc_err;
    uint32_t rx_overflow;
    uint16_t last_seq_rx;
} LoRa_Stats_t;

extern volatile uint8_t g_lora_tx_done;
extern volatile uint8_t g_lora_rx_done;
extern LoRa_Stats_t     g_lora_stats;

/**
 * @brief Reads a value from a specific LoRa register via SPI.
 * @param addr The register address to read from.
 * @return The 8-bit value read from the register.
 */
uint8_t LoRa_ReadReg(uint8_t addr);

/**
 * @brief Writes a value to a specific LoRa register via SPI.
 * @param addr The register address to write to.
 * @param value The 8-bit value to be written.
 */
void LoRa_WriteReg(uint8_t addr, uint8_t value);

/**
 * @brief Reads a burst of data from a specific LoRa register address into a buffer.
 * @param addr The starting register address.
 * @param buf Pointer to the destination buffer.
 * @param len Number of bytes to read.
 */
void LoRa_BurstRead(uint8_t addr, uint8_t* buf, uint8_t len);

/**
 * @brief Writes a burst of data from a buffer to a specific LoRa register address.
 * @param addr The starting register address.
 * @param buf Pointer to the source data buffer.
 * @param len Number of bytes to write.
 */
void LoRa_BurstWrite(uint8_t addr, const uint8_t* buf, uint8_t len);

/**
 * @brief Initializes the LoRa transceiver with standard and custom configurations.
 * @param _LoRa Pointer to the LoRa configuration structure containing RF parameters.
 * @return LORA_OK on success, or an error code indicating the failure reason.
 */
LoRa_Status_t LoRa_Init(LoRa_Config_t* _LoRa);

/**
 * @brief Handles the LoRa interrupt events from DIO pins (RxDone, TxDone, etc.).
 * @param _LoRa Pointer to the LoRa configuration structure.
 */
void LoRa_IRQHandler(LoRa_Config_t* _LoRa);

/**
 * @brief Performs a hardware reset on the LoRa module using the RESET pin.
 */
void LoRa_Reset(void);

/**
 * @brief Sets the operation mode of the LoRa transceiver.
 * @param _LoRa Pointer to the LoRa configuration structure.
 * @param mode The desired mode (e.g., SLEEP, STNBY, TX, RX).
 */
void LoRa_SetMode(LoRa_Config_t* _LoRa, int mode);

/**
 * @brief Configures the module and switches it to continuous reception mode.
 * @param _LoRa Pointer to the LoRa configuration structure.
 */
void LoRa_RxStart(LoRa_Config_t* _LoRa);

/**
 * @brief Reads the received packet data from the LoRa FIFO buffer.
 * @param _LoRa    Pointer to the LoRa configuration structure.
 * @param buf      Pointer to the buffer where received data will be stored.
 * @param max_len  Maximum capacity of the destination buffer to prevent overflow.
 * @return LoRa_Status_t LORA_OK on success, or error status code on failure.
 */
LoRa_Status_t LoRa_ReadPacketData(LoRa_Config_t* _LoRa, uint8_t* buf, uint8_t max_len);

/**
 * @brief Transmits a data packet over the air using polling mechanism for completion.
 * @param _LoRa Pointer to the LoRa configuration structure containing RF parameters.
 * @param buf Pointer to the buffer containing the raw data bytes to transmit.
 * @param len Length of the data payload in bytes.
 * @param timeout Maximum time allowed in milliseconds to wait for transmission to finish.
 * @return LoRa_Status_t LORA_OK on success, LORA_TIMEOUT if expiration occurs, or LORA_ERROR.
 */
LoRa_Status_t LoRa_Transmit(LoRa_Config_t* _LoRa, const uint8_t* buf, uint8_t len, uint32_t timeout);

/**
 * @brief Transmits data using Listen Before Talk (LBT) mechanism via CAD.
 * @param _LoRa       Pointer to the LoRa configuration structure.
 * @param buf         Pointer to the data buffer to be transmitted.
 * @param payload_len Length of the payload to transmit (Max 255 bytes).
 * @param timeout_ms  Maximum time allowed for the overall transmission process.
 * @return LoRa_Status_t LORA_OK on success, or error status code on failure.
 */
LoRa_Status_t LoRa_Transmit_LBT(LoRa_Config_t* _LoRa, const uint8_t* buf, uint8_t payload_len, uint32_t timeout_ms);

/**
 * @brief Executes a Channel Activity Detection (CAD) operation to check for preamble symbols.
 * @param _LoRa Pointer to the LoRa configuration structure.
 * @param timeout_ms Maximum time allowed in milliseconds to wait for CAD completion.
 * @return LORA_CAD_FREE if the channel is clear, LORA_CAD_BUSY if a preamble is detected, or LORA_CAD_TIMEOUT.
 */
LoRa_CAD_Status_t LoRa_DoCAD(LoRa_Config_t* _LoRa, uint32_t timeout_ms);

/**
 * @brief Configures the center frequency of the RF carrier lock.
 * @param _LoRa Pointer to the LoRa configuration structure.
 * @param freq_hz The target frequency calculated in Hertz (Hz).
 * @return LORA_OK if valid and updated, or LORA_INVALID_PARAM if out of device boundaries.
 */
LoRa_Status_t LoRa_SetFrequency(LoRa_Config_t* _LoRa, uint32_t freq_hz);

/**
 * @brief Configures the RF transmission power output and associated safety current bounds.
 * @param _LoRa Pointer to the LoRa configuration structure.
 * @param power_dbm Desired output power level scaled in dBm (supported range: 2 to 20 dBm).
 * @return LORA_OK if successful, or LORA_INVALID_PARAM if parameters are invalid.
 */
LoRa_Status_t LoRa_SetPower(LoRa_Config_t* _LoRa, uint8_t power_dbm);

/**
 * @brief Sets the Over Current Protection (OCP) threshold for the internal power amplifier.
 * @param _LoRa Pointer to the LoRa configuration structure.
 * @param current_mA Maximum allowable current limit in milliamperes (mA).
 * @return LORA_OK if validation succeeds, or LORA_INVALID_PARAM if input is out of bounds.
 */
LoRa_Status_t LoRa_SetOCP(LoRa_Config_t* _LoRa, uint8_t current_mA);

/**
 * @brief Sets the Spreading Factor (SF) for the LoRa modulation.
 * @param _LoRa Pointer to the LoRa configuration structure.
 * @param sf Spreading factor value (supported range: 6 to 12).
 * @return LORA_OK if successful, or LORA_INVALID_PARAM.
 */
LoRa_Status_t LoRa_SetSpreadingFactor(LoRa_Config_t* _LoRa, uint8_t sf);

/**
 * @brief Configures the signal bandwidth for the LoRa modem.
 * @param _LoRa Pointer to the LoRa configuration structure.
 * @param bw Enum index representing the targeted bandwidth configuration.
 * @return LORA_OK if operation is successful, or LORA_INVALID_PARAM.
 */
LoRa_Status_t LoRa_SetBandwidth(LoRa_Config_t* _LoRa, uint8_t bw);

/**
 * @brief Sets the error Coding Rate (CR) setting for corporate error corrections.
 * @param _LoRa Pointer to the LoRa configuration structure.
 * @param cr Coding rate parameter index (from CR_4_5 up to CR_4_8).
 * @return LORA_OK if successful, or LORA_INVALID_PARAM.
 */
LoRa_Status_t LoRa_SetCodingRate(LoRa_Config_t* _LoRa, uint8_t cr);

/**
 * @brief Updates the network identifier Sync Word byte inside the core transceiver.
 * @param _LoRa Pointer to the LoRa configuration structure.
 * @param syncword The byte value used to filter network frames.
 * @return LORA_OK if successful, or LORA_INVALID_PARAM.
 */
LoRa_Status_t LoRa_SetSyncWord(LoRa_Config_t* _LoRa, uint8_t syncword);

/**
 * @brief Automatically calculates and toggles Low Data Rate Optimization (LDO) register states.
 * @param _LoRa Pointer to the LoRa configuration structure.
 */
void LoRa_SetAutoLDO(LoRa_Config_t* _LoRa);

/**
 * @brief Retrieves the Signal-to-Noise Ratio (SNR) value of the last received packet.
 * @return The calculated SNR value in dB.
 */
int8_t LoRa_GetPacketSNR(void);

/**
 * @brief Retrieves the Received Signal Strength Indicator (RSSI) of the last received packet.
 * @return The RSSI value in dBm.
 */
int16_t LoRa_GetPacketRSSI(void);

/**
 * @brief Retrieves the current instantaneous RSSI value from the ambient air channel.
 * @return The immediate channel RSSI reading in dBm.
 */
int16_t LoRa_GetCurrentRSSI(void);

/**
 * @brief Resets the dynamic metrics diagnostics statistics structure data to zero.
 */
void LoRa_ResetStats(void);

/**
 * @brief Computes an 8-bit CRC checksum value over a variable length data buffer.
 * @param buf Pointer to the source data buffer.
 * @param len Length of data to compute.
 * @return Generated 8-bit CRC remainder.
 */
uint8_t LoRa_CRC8(const uint8_t* buf, uint8_t len);

extern void     LoRa_Platform_SPI_Init(void);
extern uint8_t  LoRa_Platform_SPI_Transfer(uint8_t val);
extern void     LoRa_Platform_NSS_Low(void);
extern void     LoRa_Platform_NSS_High(void);
extern void     LoRa_Platform_RESET_Low(void);
extern void     LoRa_Platform_RESET_High(void);
extern void     LoRa_Platform_DelayMs(uint32_t ms);
extern uint32_t LoRa_Platform_GetTickMs(void);

#endif