#include "main.h"
#include "LoRa.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t node_id;
    int16_t temperature;
    int16_t humidity_air;
    int16_t humidity_soil;
} SensorPacket_t;

typedef struct {
    uint8_t broadcast_id;
    uint8_t command_code;
    uint16_t network_cycle_s;
    uint16_t slot_duration_ms;
} BeaconPacket_t;

LoRa_Config_t gateway_config;

static uint32_t last_beacon_tick = 0;
volatile uint8_t g_dio0_flag = 0;

extern void mPrint(const char* format, ...);

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

    if (LoRa_Init(&gateway_config) != LORA_OK) {
        mPrint("Gateway Init Failed!\r\n");
        return;
    }

    mPrint("Gateway Init OK. Starting Master Clock...\r\n");

    /* Force an immediate beacon transmission upon startup */
    last_beacon_tick = HAL_GetTick() - 30000;
}

void Test_Gateway_Run(void) {
    /* 1. Periodic Beacon Transmission */
    if (HAL_GetTick() - last_beacon_tick >= 30000UL) {
        last_beacon_tick = HAL_GetTick();

        BeaconPacket_t beacon;
        beacon.broadcast_id     = 0x00;
        beacon.command_code     = 150;
        beacon.network_cycle_s  = 30;
        beacon.slot_duration_ms = 300;

        LoRa_Transmit(&gateway_config, (uint8_t*)&beacon, sizeof(BeaconPacket_t), 200);
        mPrint("[MASTER] Broadcasted Sync Beacon. Listening...\r\n");

        /* Flush FIFO pointer before entering RX mode to prevent buffer overflow */
        LoRa_WriteReg(REG_FIFO_ADDR_PTR, 0x00);
        LoRa_RxStart(&gateway_config);
        g_dio0_flag = 0;
    }

    /* 2. Deferred Interrupt Handling for RX Processing */
    if (g_dio0_flag == 1) {
        g_dio0_flag = 0;

        uint8_t irq_flags = LoRa_ReadReg(REG_IRQ_FLAGS);
        LoRa_WriteReg(REG_IRQ_FLAGS, irq_flags);

        if (irq_flags & 0x20) {
            mPrint("[DROP] CRC Error\r\n");
        }
        else if (irq_flags & 0x40) {
            /* Switch to Standby mode to lock the FIFO during read operations */
            LoRa_WriteReg(REG_OP_MODE, 0x80 | 0x08 | 0x01);

            uint8_t pkt_len = LoRa_ReadReg(REG_RX_NB_BYTES);
            uint8_t fifo_addr = LoRa_ReadReg(REG_FIFO_RX_CURRENT);

            /* Peek the first byte to filter out self-broadcasted beacon echoes */
            LoRa_WriteReg(REG_FIFO_ADDR_PTR, fifo_addr);
            uint8_t first_byte = LoRa_ReadReg(REG_FIFO);

            if (first_byte != 0x00 && pkt_len == sizeof(SensorPacket_t)) {
                SensorPacket_t rx_data;

                /* Reset FIFO pointer to the start of the payload before burst read */
                LoRa_WriteReg(REG_FIFO_ADDR_PTR, fifo_addr);
                LoRa_BurstRead(REG_FIFO, (uint8_t*)&rx_data, pkt_len);

                int16_t rssi = LoRa_GetPacketRSSI();
                mPrint("RX OK | Node:%d | T:%d.%d C | A:%d.%d%% | S:%d.%d%% | RSSI:%d\r\n",
                    rx_data.node_id,
                    rx_data.temperature / 100, rx_data.temperature % 100,
                    rx_data.humidity_air / 100, rx_data.humidity_air % 100,
                    rx_data.humidity_soil / 100, rx_data.humidity_soil % 100,
                    rssi);
            }
        }

        /* Mandatory FIFO cleanup to prevent the 256-byte saturation lockup */
        LoRa_WriteReg(REG_FIFO_ADDR_PTR, 0x00);
        LoRa_RxStart(&gateway_config);
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == LORA_DIO0_Pin) {
        g_dio0_flag = 1;
    }
}
