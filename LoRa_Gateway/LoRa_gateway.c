#include "main.h"
#include "LoRa.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef struct {
    uint8_t node_id;
    int16_t temperature;
    int16_t humidity_air;
    int16_t humidity_soil;
} SensorPacket_t;

LoRa_Config_t gateway_config;

static SensorPacket_t rx_data;
static uint8_t rx_buffer[64];

volatile uint8_t g_gateway_packet_ready = 0;

extern void mPrint(const char* format, ...);
extern volatile uint8_t g_lora_rx_done;

/**
 * @brief Initializes the LoRa gateway configuration parameters and starts listening mode.
 */
void Test_Gateway_Init(void) {
    gateway_config.frequency             = 433000000UL;
    gateway_config.preamble              = 10;
    gateway_config.spredingFactor        = 7;
    gateway_config.bandWidth             = 7;
    gateway_config.crcRate               = 1;
    gateway_config.power                 = 20;
    gateway_config.overCurrentProtection = 120;
    gateway_config.syncWord              = 0x12;
    gateway_config.enableCRC             = 1;
    gateway_config.implicitHeader        = 0;

    if (LoRa_Init(&gateway_config) == LORA_OK) {
        mPrint("LoRa Gateway Init Success (Safe Double-Buffer Interrupt)!\r\n");
    } else {
        mPrint("LoRa Gateway Init Failed!\r\n");
        return;
    }

    LoRa_RxStart(&gateway_config);
    mPrint("Gateway is scanning air for packets...\r\n");
}

/**
 * @brief Main background process for the gateway, executed continuously inside the super loop.
 * Processes incoming sensor telemetry data when packets are ready in RAM.
 */
void Test_Gateway_Run(void) {
    if (g_gateway_packet_ready == 1) {
        g_gateway_packet_ready = 0;

        LoRa_ReadPacketData(&gateway_config, rx_buffer, sizeof(rx_buffer));

        memcpy(&rx_data, rx_buffer, sizeof(SensorPacket_t));

        int16_t packet_rssi = LoRa_GetPacketRSSI();
        int8_t  packet_snr  = LoRa_GetPacketSNR();

        mPrint("RX OK | Node:%d Temp:%d Air:%d Soil:%d | RSSI:%d dBm | SNR:%d dB\r\n",
                rx_data.node_id, rx_data.temperature, rx_data.humidity_air, rx_data.humidity_soil,
                packet_rssi, packet_snr);
    }
}

/**
 * @brief External Interrupt (EXTI) line callback implementation for the LoRa DIO0 pin.
 * Handles time-critical hardware IRQ flags, extracts FIFO data, and re-triggers RX mode.
 * @param GPIO_Pin Specifies the pins connected to the EXTI line.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == LORA_DIO0_Pin) {
        uint8_t irq_flags = LoRa_ReadReg(REG_IRQ_FLAGS);

        LoRa_WriteReg(REG_IRQ_FLAGS, irq_flags);

        if (irq_flags & 0x20) {
            g_lora_stats.rx_crc_err++;
            LoRa_RxStart(&gateway_config);
            return;
        }

        if (irq_flags & 0x40) {
            LoRa_WriteReg(REG_OP_MODE, 0x80 | 0x08 | 0x01);

            uint8_t pkt_len   = LoRa_ReadReg(REG_RX_NB_BYTES);
            uint8_t fifo_addr = LoRa_ReadReg(REG_FIFO_RX_CURRENT);

            if (pkt_len == sizeof(SensorPacket_t)) {
                LoRa_WriteReg(REG_FIFO_ADDR_PTR, fifo_addr);
                LoRa_BurstRead(REG_FIFO, rx_buffer, pkt_len);

                g_lora_rx_done = 1;
                g_gateway_packet_ready = 1;
            }

            LoRa_RxStart(&gateway_config);
        }
    }
}