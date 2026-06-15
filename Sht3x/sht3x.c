#include "sht3x.h"
#include <stdint.h>

uint8_t SHT3x_CheckCrc8(uint8_t *data, uint8_t nBytes) {
    uint8_t crc = SHT3X_CRC_INIT; // Giá trị khởi tạo [cite: 221]
    for (uint8_t byteIndex = 0; byteIndex < nBytes; byteIndex++) {
        crc ^= data[byteIndex];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ SHT3X_CRC_POLYNOMIAL; // Đa thức 0x31 [cite: 221]
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}

SHT3x_status_t SHT3x_write_command(uint8_t address, uint16_t command, uint32_t timeout) {
    uint8_t write_addr = address & 0xFE;
    uint8_t cmd_buffer[2];
    
    // Tách lệnh 16-bit thành mảng 2 byte để gửi đồng thời
    cmd_buffer[0] = (uint8_t)((command >> 8) & 0xFF); // MSB
    cmd_buffer[1] = (uint8_t)(command & 0xFF);        // LSB

    // Gửi một lần duy nhất: Địa chỉ + Mảng lệnh 2 byte
    if (SHT3x_Platform_I2C_Transmit(write_addr, cmd_buffer, 2, timeout) != SHT3X_OK) {
        return SHT3X_ERROR;
    }
    
    return SHT3X_OK;
}

SHT3x_status_t SHT3x_read_bytes(uint8_t address, uint16_t length, uint32_t timeout) {
    uint8_t read_addr = address | 0x01;
    uint8_t buffer[SHT3X_buffer_size];

    // Truyền thêm độ dài dữ liệu 'length' vào hàm nhận để đọc đủ 6 byte từ SHT3x
    if (SHT3x_Platform_I2C_Receive(read_addr, buffer, length, timeout) != SHT3X_OK) {
        return SHT3X_ERROR;
    }
    for(int i = 0; i < 2; i++){
        uint8_t* p = (buffer + i*3);
        if(SHT3x_CheckCrc8(p,2) = SHT3x_OK) return SHT3x_ERROR;
    }
    uint16_t temp
    return SHT3X_OK;
}


void SHT3x_Read_Single_Shot(SHT3x_Repeatability_t repeat, SHT3x_Clock_stretching_t cs){
    uint16_t cmd;
    uint32_t delay_ms;
    switch (cs){
        case SHT3X_CS_ENABLED:
            switch (repeat) {
                case SHT3X_REPEAT_LOW:
                    cmd      = SHT3X_CMD_MEAS_CS_LOW;
                    delay_ms = SHT3X_MEAS_DELAY_LOW_MS;
                    break;
                case SHT3X_REPEAT_MEDIUM:
                    cmd      = SHT3X_CMD_MEAS_CS_MEDIUM;
                    delay_ms = SHT3X_MEAS_DELAY_MEDIUM_MS;
                    break;
                case SHT3X_REPEAT_HIGH:
                default:
                    cmd      = SHT3X_CMD_MEAS_CS_HIGH;
                    delay_ms = SHT3X_MEAS_DELAY_HIGH_MS;
                    break;
            }
        case SHT3X_CS_DISABLED:
            switch (repeat) {
                case SHT3X_REPEAT_LOW:
                    cmd      = SHT3X_CMD_MEAS_LOW;
                    delay_ms = SHT3X_MEAS_DELAY_LOW_MS;
                    break;
                case SHT3X_REPEAT_MEDIUM:
                    cmd      = SHT3X_CMD_MEAS_MEDIUM;
                    delay_ms = SHT3X_MEAS_DELAY_MEDIUM_MS;
                    break;
                case SHT3X_REPEAT_HIGH:
                default:
                    cmd      = SHT3X_CMD_MEAS_HIGH;
                    delay_ms = SHT3X_MEAS_DELAY_HIGH_MS;
                    break;
            }         
    }
    
}