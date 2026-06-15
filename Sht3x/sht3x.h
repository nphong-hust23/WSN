/* I2C Addresses */
#define SHT3X_I2C_ADDR_GND        (0x44 << 1)    // Default I2C address, ADDR pin connected to logic low
#define SHT3X_I2C_ADDR_VDD        (0x45 << 1)   // I2C address, ADDR pin connected to logic high

/* Single Shot Data Acquisition Mode Commands */
#define SHT3X_CMD_MEAS_CS_HIGH    0x2C06  // Clock stretching enabled, high repeatability
#define SHT3X_CMD_MEAS_CS_MED     0x2C0D  // Clock stretching enabled, medium repeatability
#define SHT3X_CMD_MEAS_CS_LOW     0x2C10  // Clock stretching enabled, low repeatability

#define SHT3X_CMD_MEAS_HIGH  0x2400  // Clock stretching disabled, high repeatability
#define SHT3X_CMD_MEAS_MED   0x240B  // Clock stretching disabled, medium repeatability
#define SHT3X_CMD_MEAS_LOW   0x2416  // Clock stretching disabled, low repeatability

/*Max Measurement Duration in ms (Valid for VDD 2.4V to 5.5V)uint*/
#define SHT3X_MEAS_DELAY_LOW_MS     4
#define SHT3X_MEAS_DELAY_MEDIUM_MS  6
#define SHT3X_MEAS_DELAY_HIGH_MS    15

/* Periodic Data Acquisition Mode Commands */
#define SHT3X_CMD_PERI_0_5_HIGH   0x2032  // 0.5 measurements per second (mps), high repeatability
#define SHT3X_CMD_PERI_0_5_MED    0x2024  // 0.5 mps, medium repeatability
#define SHT3X_CMD_PERI_0_5_LOW    0x202F  // 0.5 mps, low repeatability

#define SHT3X_CMD_PERI_1_HIGH     0x2130  // 1 mps, high repeatability
#define SHT3X_CMD_PERI_1_MED      0x2126  // 1 mps, medium repeatability
#define SHT3X_CMD_PERI_1_LOW      0x212D  // 1 mps, low repeatability

#define SHT3X_CMD_PERI_2_HIGH     0x2236  // 2 mps, high repeatability
#define SHT3X_CMD_PERI_2_MED      0x2220  // 2 mps, medium repeatability
#define SHT3X_CMD_PERI_2_LOW      0x222B  // 2 mps, low repeatability

#define SHT3X_CMD_PERI_4_HIGH     0x2334  // 4 mps, high repeatability
#define SHT3X_CMD_PERI_4_MED      0x2322  // 4 mps, medium repeatability
#define SHT3X_CMD_PERI_4_LOW      0x2329  // 4 mps, low repeatability

#define SHT3X_CMD_PERI_10_HIGH    0x2737  // 10 mps, high repeatability
#define SHT3X_CMD_PERI_10_MED     0x2721  // 10 mps, medium repeatability
#define SHT3X_CMD_PERI_10_LOW     0x272A  // 10 mps, low repeatability

/* System, Periodic Control and Advanced Commands */
#define SHT3X_CMD_FETCH_DATA      0xE000  // Readout of measurement results for periodic mode
#define SHT3X_CMD_ART             0x2B32  // Periodic measurement with Accelerated Response Time (ART) feature
#define SHT3X_CMD_BREAK           0x3093  // Stop periodic data acquisition mode / break command
#define SHT3X_CMD_SOFT_RESET      0x30A2  // Soft reset / re-initialization
#define SHT3X_GENERAL_CALL_ADDR   0x00    // General call address for I2C
#define SHT3X_GENERAL_CALL_RESET  0x06    // Reset command using the general call address

/* Heater Commands */
#define SHT3X_CMD_HEATER_ENABLE   0x306D  // Enable internal heater
#define SHT3X_CMD_HEATER_DISABLE  0x3066  // Disable internal heater

/* Status Register Commands */
#define SHT3X_CMD_READ_STATUS     0xF32D  // Read out of status register
#define SHT3X_CMD_CLEAR_STATUS    0x3041  // Clear all flags in the status register

/* CRC-8 Constants */
#define SHT3X_CRC_POLYNOMIAL      0x31    // CRC-8 polynomial (x^8 + x^5 + x^4 + 1)
#define SHT3X_CRC_INIT            0xFF    // CRC-8 initialization value

#define SHT3X_BUFFER_SIZE         0x06
typedef enum{
    SHT3x_OK     = 0,
    SHT3x_ERROR    = -1, 
}SHT3x_status_t;

typedef enum {
    SHT3X_REPEAT_LOW    = 0,
    SHT3X_REPEAT_MEDIUM = 1,
    SHT3X_REPEAT_HIGH   = 2
} SHT3x_Repeatability_t;

typedef enum{
    SHT3X_CS_ENABLED    = 0,
    SHT3X_CS_DISABLED   = 1
}SHT3x_Clock_stretching_t

SHT3x_status SHT3x_write_byte();
SHT3x_status SHT3x_read_byte();

uint8_t SHT3x_CheckCrc(uint8_t *data, uint8_t nBytes);

// Thêm tham số mảng dữ liệu (p_data) và độ dài (size) vào hàm Platform Transmit
extern SHT3x_status SHT3x_Platform_I2C_Transmit(uint8_t address, uint8_t* p_data, uint16_t size, uint32_t timeout);

// Thêm tham số độ dài (size) vào hàm Platform Receive để báo cho I2C biết cần đọc bao nhiêu byte
extern SHT3x_status SHT3x_Platform_I2C_Receive(uint8_t address, uint8_t* buffer, uint16_t size, uint32_t timeout);

// Hàm ghi lệnh xuống SHT3x (Cập nhật đúng tham số)
SHT3x_status SHT3x_write_command(uint8_t address, uint16_t command, uint32_t timeout);
SHT3x_status SHT3x_read_bytes(uint8_t address, uint8_t* buffer, uint16_t length, uint32_t timeout);
SHT3x_status SHT3x_read_value();