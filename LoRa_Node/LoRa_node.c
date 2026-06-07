#include "LoRa.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef struct
{
    uint8_t node_id;
    int16_t temperature;
    int16_t humidity_air;
    int16_t humidity_soil;
} SensorPacket_t;

LoRa_Config_t node_config;
extern void mPrint(const char* format, ...);

static int16_t simulated_temp          = 2845;
static int16_t simulated_humidity_air  = 6520;
static int16_t simulated_humidity_soil = 3040;

/**
 * @brief Initializes the LoRa node configuration parameters and sets the device to low-power sleep mode.
 */
void Test_Node_Init(void) {
    node_config.frequency             = 433000000UL;
    node_config.preamble              = 10;
    node_config.spredingFactor        = 7;
    node_config.bandWidth             = 7;
    node_config.crcRate               = 1;
    node_config.power                 = 20;
    node_config.overCurrentProtection = 120;
    node_config.syncWord              = 0x12;
    node_config.enableCRC             = 1;
    node_config.implicitHeader        = 0;
    node_config.nodeId                = 0x02;

    if (LoRa_Init(&node_config) != LORA_OK) {
        mPrint("LoRa Node Init Failed!\r\n");
        return;
    }
    mPrint("LoRa Node Init Success (POLLING MODE ACTIVE)!\r\n");

    LoRa_SetMode(&node_config, SLEEP_MODE);
}

/**
 * @brief Main periodic process for the node, executed inside the super loop.
 * Wakes up the transceiver, updates telemetry data, transmits via polling, and returns to sleep.
 */
void Test_Node_Run(void) {
    char uart_buf[128];

    LoRa_SetMode(&node_config, STNBY_MODE);
    HAL_Delay(5);

    simulated_temp += 5;
    if (simulated_temp > 4000) simulated_temp = 2500;

    SensorPacket_t tx_packet;
    tx_packet.node_id       = node_config.nodeId;
    tx_packet.temperature   = simulated_temp;
    tx_packet.humidity_air  = simulated_humidity_air;
    tx_packet.humidity_soil = simulated_humidity_soil;

    snprintf(uart_buf, sizeof(uart_buf),
             "\r\n[Polling TX] Node:%d Temp:%d Air:%d Soil:%d\r\n",
             tx_packet.node_id, tx_packet.temperature, tx_packet.humidity_air, tx_packet.humidity_soil);
    mPrint(uart_buf);

    LoRa_Status_t status = LoRa_Transmit(&node_config, (uint8_t*)&tx_packet, sizeof(SensorPacket_t), 1000);

    if (status == LORA_OK) {
        mPrint("[TX OK] Packet pushed to air successfully!\r\n");
    } else if (status == LORA_TIMEOUT) {
        mPrint("[TX Timeout] Transmit failed!\r\n");
    } else {
        mPrint("[TX Error] Unknown error!\r\n");
    }

    LoRa_SetMode(&node_config, SLEEP_MODE);

    HAL_Delay(5000);
}