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

LoRa_Status_t     LoRa_Init(LoRa_Config_t* _LoRa);
void              LoRa_IRQHandler(LoRa_Config_t* _LoRa);
void              LoRa_Reset(void);
void              LoRa_SetMode(LoRa_Config_t* _LoRa, int mode);
void              LoRa_RxStart(LoRa_Config_t* _LoRa);

LoRa_Status_t LoRa_ReadPacketData(LoRa_Config_t* _LoRa, uint8_t* buf,
                                  uint8_t max_len);

LoRa_Status_t     LoRa_Transmit(LoRa_Config_t* _LoRa, const uint8_t* buf,
                                  uint8_t len, uint32_t timeout);
LoRa_Status_t     LoRa_Transmit_LBT(LoRa_Config_t* _LoRa, const uint8_t* buf,
                                      uint8_t payload_len, uint32_t timeout_ms);
LoRa_CAD_Status_t LoRa_DoCAD(LoRa_Config_t* _LoRa, uint32_t timeout_ms);

LoRa_Status_t     LoRa_SetFrequency(LoRa_Config_t* _LoRa, uint32_t freq_hz);
LoRa_Status_t     LoRa_SetPower(LoRa_Config_t* _LoRa, uint8_t power_dbm);
LoRa_Status_t     LoRa_SetOCP(LoRa_Config_t* _LoRa, uint8_t current_mA);
LoRa_Status_t     LoRa_SetSpreadingFactor(LoRa_Config_t* _LoRa, uint8_t sf);
LoRa_Status_t     LoRa_SetBandwidth(LoRa_Config_t* _LoRa, uint8_t bw);
LoRa_Status_t     LoRa_SetCodingRate(LoRa_Config_t* _LoRa, uint8_t cr);
LoRa_Status_t     LoRa_SetSyncWord(LoRa_Config_t* _LoRa, uint8_t syncword);
void              LoRa_SetAutoLDO(LoRa_Config_t* _LoRa);

int8_t            LoRa_GetPacketSNR(void);
int16_t           LoRa_GetPacketRSSI(void);
int16_t           LoRa_GetCurrentRSSI(void);
void              LoRa_ResetStats(void);
uint8_t           LoRa_CRC8(const uint8_t* buf, uint8_t len);

extern void     LoRa_Platform_SPI_Init(void);
extern uint8_t  LoRa_Platform_SPI_Transfer(uint8_t val);
extern void     LoRa_Platform_NSS_Low(void);
extern void     LoRa_Platform_NSS_High(void);
extern void     LoRa_Platform_RESET_Low(void);
extern void     LoRa_Platform_RESET_High(void);
extern void     LoRa_Platform_DelayMs(uint32_t ms);
extern uint32_t LoRa_Platform_GetTickMs(void);

#endif
