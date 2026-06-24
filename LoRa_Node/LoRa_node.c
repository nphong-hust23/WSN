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

typedef enum {
    STATE_SYNC_BEACON = 0,
    STATE_WAIT_SLOT_TX,
    STATE_SLEEP_UNTIL_NEXT
} NodeState_t;

LoRa_Config_t node_config;

extern RTC_HandleTypeDef hrtc;
extern __IO uint32_t uwTick;
extern void SystemClock_Config(void);
extern void mPrint(const char* format, ...);

static int16_t simulated_temp = 2450;
volatile uint8_t g_dio0_flag = 0;

static uint16_t cfg_cycle_s = 30;
static uint16_t cfg_slot_ms = 300;
static NodeState_t current_state = STATE_SYNC_BEACON;

static void RTC_Set_Wakeup_Alarm(uint32_t seconds) {
    if (seconds == 0) return;
    RTC_AlarmTypeDef sAlarm = {0};
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    /* Reading Date is required to unlock the RTC shadow registers */
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    uint32_t total_s = sTime.Hours * 3600 + sTime.Minutes * 60 + sTime.Seconds + seconds;
    total_s %= 86400;

    sAlarm.AlarmTime.Hours   = total_s / 3600;
    sAlarm.AlarmTime.Minutes = (total_s % 3600) / 60;
    sAlarm.AlarmTime.Seconds = total_s % 60;
    sAlarm.Alarm = RTC_ALARM_A;
    HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BIN);
}

static void DeepSleep_With_Compensation(uint32_t sleep_sec) {
    if (sleep_sec == 0) return;

    HAL_SuspendTick();
    RTC_Set_Wakeup_Alarm(sleep_sec);
    HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFI);

    /* Wake-up sequence: Restore SysTick and System Clock */
    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
    HAL_ResumeTick();

    if (__HAL_RCC_GET_SYSCLK_SOURCE() != RCC_SYSCLKSOURCE_STATUS_PLLCLK) {
        __HAL_RCC_HSE_CONFIG(RCC_HSE_ON);
        while(__HAL_RCC_GET_FLAG(RCC_FLAG_HSERDY) == RESET);
        __HAL_RCC_PLL_ENABLE();
        while(__HAL_RCC_GET_FLAG(RCC_FLAG_PLLRDY) == RESET);
        SystemClock_Config();
    }

    /* Add the elapsed sleep duration back to the system tick to maintain time sync */
    uwTick += (sleep_sec * 1000);
}

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
    node_config.nodeId                = 20;

    LoRa_Init(&node_config);
    mPrint("Node Init OK. Entering SYNC State...\r\n");

    LoRa_WriteReg(REG_FIFO_ADDR_PTR, 0x00);
    LoRa_RxStart(&node_config);
    current_state = STATE_SYNC_BEACON;
}

void Test_Node_Run(void) {

    switch (current_state) {

        /* State 0: Idle and wait for the Master Gateway beacon */
        case STATE_SYNC_BEACON:
            if (g_dio0_flag) {
                g_dio0_flag = 0;
                uint8_t irq = LoRa_ReadReg(REG_IRQ_FLAGS);
                LoRa_WriteReg(REG_IRQ_FLAGS, irq);

                if ((irq & 0x40) && !(irq & 0x20)) {
                    LoRa_WriteReg(REG_OP_MODE, 0x80 | 0x08 | 0x01);
                    if (LoRa_ReadReg(REG_RX_NB_BYTES) == sizeof(BeaconPacket_t)) {
                        BeaconPacket_t beacon;
                        LoRa_WriteReg(REG_FIFO_ADDR_PTR, LoRa_ReadReg(REG_FIFO_RX_CURRENT));
                        LoRa_BurstRead(REG_FIFO, (uint8_t*)&beacon, sizeof(BeaconPacket_t));

                        if (beacon.broadcast_id == 0x00 && beacon.command_code == 150) {
                            cfg_cycle_s = beacon.network_cycle_s;
                            cfg_slot_ms = beacon.slot_duration_ms;

                            /* Safely reset the system tick reference */
                            __disable_irq();
                            uwTick = 0;
                            __enable_irq();

                            current_state = STATE_WAIT_SLOT_TX;
                            mPrint("\r\n[SYNC] Beacon Rx! uwTick reset to 0.\r\n");
                        }
                    }
                }
                /* Flush the FIFO to prevent hardware buffer saturation */
                LoRa_WriteReg(REG_FIFO_ADDR_PTR, 0x00);
                LoRa_RxStart(&node_config);
            }
            break;

        /* State 1: Calculate wait time and sleep until assigned slot */
        case STATE_WAIT_SLOT_TX: {
            uint32_t target_tx_ms = (node_config.nodeId - 1) * cfg_slot_ms;

            while (uwTick < target_tx_ms) {
                uint32_t remain_ms = target_tx_ms - uwTick;
                if (remain_ms >= 2000) {
                    uint32_t sleep_s = (remain_ms / 1000) - 1;
                    mPrint("[SLOT] Deep sleeping for %d s to wait for slot...\r\n", sleep_s);
                    DeepSleep_With_Compensation(sleep_s);
                } else {
                    HAL_Delay(1);
                }
            }

            LoRa_SetMode(&node_config, STNBY_MODE);
            SensorPacket_t tx_pkt = {node_config.nodeId, simulated_temp += 5, 6000, 4000};
            LoRa_Transmit(&node_config, (uint8_t*)&tx_pkt, sizeof(SensorPacket_t), 200);

            mPrint("[TX] Pushed data at %d ms\r\n", uwTick);
            current_state = STATE_SLEEP_UNTIL_NEXT;
            break;
        }

        /* State 2: Enter deep sleep for the remainder of the network cycle */
        case STATE_SLEEP_UNTIL_NEXT: {
            uint32_t target_wakeup_ms = (cfg_cycle_s * 1000) - 2000;

            mPrint("[SLEEP] Sleeping until %d ms to catch next Beacon...\r\n", target_wakeup_ms);

            while (uwTick < target_wakeup_ms) {
                uint32_t remain_ms = target_wakeup_ms - uwTick;
                if (remain_ms >= 2000) {
                    uint32_t sleep_s = (remain_ms / 1000) - 1;
                    DeepSleep_With_Compensation(sleep_s);
                } else {
                    HAL_Delay(1);
                }
            }

            mPrint("[GUARD] Guard awake. Receiver opened!\r\n");

            /* Flush FIFO and enable receiver to catch the upcoming beacon */
            LoRa_WriteReg(REG_FIFO_ADDR_PTR, 0x00);
            LoRa_RxStart(&node_config);
            g_dio0_flag = 0;
            current_state = STATE_SYNC_BEACON;
            break;
        }
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == LORA_DIO0_Pin) g_dio0_flag = 1;
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc) {}
