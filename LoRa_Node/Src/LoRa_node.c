#include "main.h"
#include "LoRa.h"
#include "sht3x.h"
#include "soil_sensor.h"
#include <stdio.h>
#include <string.h>

typedef struct __attribute__((packed)) {
    uint8_t node_id;
    int16_t temperature;
    int16_t humidity_air;
    int16_t humidity_soil;
} SensorPacket_t;

typedef struct __attribute__((packed)) {
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
extern ADC_HandleTypeDef hadc1; /* ADC peripheral handle from main.c */
extern __IO uint32_t uwTick;
extern void SystemClock_Config(void);
extern void mPrint(const char* format, ...);

volatile uint8_t g_dio0_flag = 0;

static uint16_t cfg_cycle_s = 30;
static uint16_t cfg_slot_ms = 300;
static NodeState_t current_state = STATE_SYNC_BEACON;

static void RTC_Set_Wakeup_Alarm(uint32_t seconds) {
    if (seconds == 0) return;
    RTC_AlarmTypeDef sAlarm = {0};
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

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

    /* Wake-up sequence */
    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
    HAL_ResumeTick();

    if (__HAL_RCC_GET_SYSCLK_SOURCE() != RCC_SYSCLKSOURCE_STATUS_PLLCLK) {
        __HAL_RCC_HSE_CONFIG(RCC_HSE_ON);
        while(__HAL_RCC_GET_FLAG(RCC_FLAG_HSERDY) == RESET);
        __HAL_RCC_PLL_ENABLE();
        while(__HAL_RCC_GET_FLAG(RCC_FLAG_PLLRDY) == RESET);
        SystemClock_Config();
    }

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


    /* 2. Init Modules */
    if (LoRa_Init(&node_config) != LORA_OK) {
        mPrint("[ERROR] LoRa Init Failed!\r\n");
    } else {
        mPrint("[OK] LoRa Init Success.\r\n");
    }

    if (SHT3x_Init(SHT3X_I2C_ADDR_GND, 100) == SHT3X_OK) {
        mPrint("[OK] SHT3x Sensor Ready.\r\n");
    } else {
        mPrint("[ERROR] SHT3x Init Failed!\r\n");
    }

    SoilSensor_Init(&hadc1);
    mPrint("[OK] ADC DMA Soil Sensor Ready.\r\n");

    LoRa_WriteReg(REG_FIFO_ADDR_PTR, 0x00);
    LoRa_RxStart(&node_config);
    current_state = STATE_SYNC_BEACON;
}

void Test_Node_Run(void) {
    switch (current_state) {

        case STATE_SYNC_BEACON:
            /* ORPHAN FALLBACK: Deep sleep if Gateway is dead for more than (Cycle + 5) seconds */
            if (uwTick > ((cfg_cycle_s + 5) * 1000)) {
                mPrint("[ORPHAN] Gateway lost! Deep sleeping for 5 mins to save battery...\r\n");

                LoRa_SetMode(&node_config, STNBY_MODE);
                DeepSleep_With_Compensation(300); /* Sleep 300s */

                mPrint("[WAKEUP] Re-sniffing for Gateway beacon...\r\n");
                uwTick = 0; /* Reset timeout watchdog */

                LoRa_WriteReg(REG_FIFO_ADDR_PTR, 0x00);
                LoRa_RxStart(&node_config);
            }

            /* Regular Beacon Reception Logic */
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

                            __disable_irq();
                            uwTick = 0;
                            __enable_irq();

                            current_state = STATE_WAIT_SLOT_TX;
                            mPrint("\r\n[SYNC] Beacon Received! Clock synced.\r\n");
                        }
                    }
                }
                LoRa_WriteReg(REG_FIFO_ADDR_PTR, 0x00);
                LoRa_RxStart(&node_config);
            }
            break;

        case STATE_WAIT_SLOT_TX: {
            uint32_t target_tx_ms = (node_config.nodeId - 1) * cfg_slot_ms;

            while (uwTick < target_tx_ms) {
                uint32_t remain_ms = target_tx_ms - uwTick;
                if (remain_ms >= 2000) {
                    uint32_t sleep_s = (remain_ms / 1000) - 1;
                    DeepSleep_With_Compensation(sleep_s);
                } else {
                    HAL_Delay(1);
                }
            }

            LoRa_SetMode(&node_config, STNBY_MODE);

            /* ========================================== */
            /* --- FETCH REAL SENSOR DATA HERE        --- */
            /* ========================================== */
            SHT3x_Data_t real_sht_data;
            int16_t tx_temp = 0, tx_humi_air = 0;

            /* Lấy dữ liệu SHT3x qua I2C */
            if (SHT3x_ReadSingleShot(SHT3X_I2C_ADDR_GND, SHT3X_REPEAT_HIGH, SHT3X_CS_DISABLED, &real_sht_data, 100) == SHT3X_OK) {
                tx_temp     = (int16_t)(real_sht_data.temperature * 100.0f);
                tx_humi_air = (int16_t)(real_sht_data.humidity * 100.0f);
            } else {
                mPrint("[WARN] SHT3x read failed. Using 0 values.\r\n");
            }

            /* Lấy dữ liệu Soil Moisture từ RAM (đã được DMA cập nhật ngầm) */
            uint8_t soil_percent = SoilSensor_GetPercentage();
            int16_t tx_humi_soil = (int16_t)(soil_percent * 100);

            /* Đóng gói vào Payload */
            SensorPacket_t tx_pkt = {
                node_config.nodeId,
                tx_temp,
                tx_humi_air,
                tx_humi_soil
            };

            /* Phát sóng */
            LoRa_Transmit(&node_config, (uint8_t*)&tx_pkt, sizeof(SensorPacket_t), 2000);

            mPrint("[TX] Sent -> Temp: %d.%d C | Air: %d.%d %% | Soil: %d %%\r\n",
                    tx_temp/100, tx_temp%100, tx_humi_air/100, tx_humi_air%100, soil_percent);

            current_state = STATE_SLEEP_UNTIL_NEXT;
            break;
        }

        case STATE_SLEEP_UNTIL_NEXT: {
            /* Guard Time: Wake up 6 seconds early to compensate LSI drift */
            uint32_t target_wakeup_ms = (cfg_cycle_s * 1000) - 2000;

            mPrint("[SLEEP] Sleeping until %d ms...\r\n", target_wakeup_ms);

            while (uwTick < target_wakeup_ms) {
                uint32_t remain_ms = target_wakeup_ms - uwTick;
                if (remain_ms >= 2000) {
                    uint32_t sleep_s = (remain_ms / 1000) - 1;
                    DeepSleep_With_Compensation(sleep_s);
                } else {
                    HAL_Delay(1);
                }
            }

            mPrint("[GUARD] Node awake. Sniffing for beacon...\r\n");
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
