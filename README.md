```markdown
# SX1276 LoRa Driver User Guide (Interrupt, LBT & Network Application Example)

This documentation provides a comprehensive guide on how to integrate and deploy this hardware-agnostic Semtech SX1276 LoRa driver into your embedded project (compatible with STM32, ESP32, Arduino, AVR, or any other MCU platform).

---

## 🛠 Step 1: Implement the Platform Abstraction Layer (PAL)

This driver is fully decoupled from the underlying hardware. Your first task is to provide the actual firmware implementation for SPI communication, GPIO controls (NSS, RESET), and time delays inside your board support files (e.g., `main.c` or `hardware.c`):

```c
void LoRa_Platform_SPI_Init(void) {
    // Initialize your MCU SPI peripheral here (Recommended clock speed < 10MHz)
}

uint8_t LoRa_Platform_SPI_Transfer(uint8_t val) {
    // Send and receive 1 byte simultaneously over SPI
    // STM32 Example: HAL_SPI_TransmitReceive(&hspi1, &val, &rx_val, 1, 100); return rx_val;
    return 0; 
}

void LoRa_Platform_NSS_Low(void) {
    // Pull the CS/NSS GPIO pin LOW to select the LoRa transceiver
}

void LoRa_Platform_NSS_High(void) {
    // Pull the CS/NSS GPIO pin HIGH to deselect the LoRa transceiver
}

void LoRa_Platform_RESET_Low(void) {
    // Pull the hardware RESET pin LOW
}

void LoRa_Platform_RESET_High(void) {
    // Pull the hardware RESET pin HIGH
}

void LoRa_Platform_DelayMs(uint32_t ms) {
    // System millisecond delay function
}

uint32_t LoRa_Platform_GetTickMs(void) {
    // Return elapsed milliseconds since power-on (used for internal timeouts)
    return 0; 
}

```

---

## ⚙️ Step 2: Device Configuration & RX Mode Activation

Instantiate a `LoRa_Config_t` configuration structure to establish your RF operating parameters, call the core driver initialization routine, and switch the module into automated listening mode.

```c
#include "LoRa_gemini2.h"

// 1. Declare the global configuration structure
LoRa_Config_t lora_device = {
    .frequency             = 433000000UL, // Carrier Frequency: 433 MHz
    .preamble              = 8,           // Standard preamble symbols
    .spredingFactor        = 7,           // SF7 (Balanced speed and range)
    .bandWidth             = BW_125KHz,   // Signal Bandwidth: 125 KHz
    .crcRate               = CR_4_5,      // Error Coding Rate: 4/5
    .power                 = 14,          // Transmit Output Power: 14 dBm
    .overCurrentProtection = 100,         // Internal OCP threshold: 100 mA
    .syncWord              = 0x12,        // Network Identifier (Private Network)
    .nodeId                = 1,           // Local Node ID (used for LBT random backoff calculation)
    .enableCRC             = 1,           // Enable hardware packet payload CRC check
    .implicitHeader        = 0            // Use Explicit Header mode (LoRa Standard)
};

// 2. RX Callback executing automatically upon valid packet reception
void lora_on_receive_callback(uint8_t packet_len) {
    // Note: This runs within the hardware Interrupt Service Routine (ISR) context.
    // Keep it lean! Set a flag or notify an OS task queue; do NOT execute blocking actions here.
}

void Application_Init(void) {
    // Initialize the core transceiver driver
    if (LoRa_Init(&lora_device) == LORA_OK) {
        // Register the user-defined asynchronous RX callback
        LoRa_RegisterRxCallback(&lora_device, lora_on_receive_callback);
        
        // Enter Continuous Reception (Continuous RX) mode
        LoRa_RxStart(&lora_device);
    } else {
        // Device initialization failed (Check SPI wiring or configuration bounds)
        while(1);
    }
}

```

---

## ⚡ Step 3: Configure the DIO0 External Interrupt (Crucial)

The SX1276 transceiver flags critical events (Transmission Done, Reception Done, CRC Corruption) externally via its physical `DIO0` hardware pin. You must configure the host MCU GPIO pin connected to `DIO0` as an **External Interrupt triggered on the Rising Edge**.

Forward the raw execution from your MCU's GPIO ISR context straight to the driver's handler:

```c
// Example of your host MCU GPIO Interrupt Service Routine (ISR)
void My_MCU_GPIO_Interrupt_Handler(void) {
    // Delegate the processing straight to the LoRa Driver ISR Handler
    LoRa_IRQHandler(&lora_device);
}

```

---

## 🔄 Step 4: Processing Packet Data inside the Main Loop

Once the external interrupt fires and validates a frame, the driver automatically resets the hardware pointers and raises the flag `g_lora_rx_done = 1`. In your non-blocking main `while(1)` loop or an isolated RTOS task, poll this flag to extract payloads:

```c
void Application_Loop(void) {
    uint8_t  rx_buffer[255];
    uint8_t  actual_payload_len = 0;
    uint16_t packet_sequence_num = 0;

    // Check if the driver has flagged a newly arrived valid packet
    if (g_lora_rx_done) {
        // Safe, bound-checked read operation extracting clean data
        LoRa_Status_t status = LoRa_ReadPacketData(
            &lora_device, 
            rx_buffer, 
            sizeof(rx_buffer), 
            &actual_payload_len, 
            &packet_sequence_num
        );

        if (status == LORA_OK && actual_payload_len > 0) {
            // Process clean payload data in `rx_buffer` here...
            
            // Extract over-the-air link quality diagnostics for the packet
            int16_t rssi = LoRa_GetPacketRSSI(); // Signal strength indicator (dBm)
            int8_t  snr  = LoRa_GetPacketSNR();  // Signal-to-Noise ratio (dB)
        }
    }
}

```

---

## 📡 Step 5: Reliable Medium Access (LBT - Listen Before Talk)

To mitigate collisions and data losses in shared, highly congested industrial RF channels, utilize the **Listen-Before-Talk (LBT)** transmission API. The driver evaluates ambient air activity (CAD); if the medium is occupied, it implements a pseudo-random delay algorithm (Backoff) before retrying. It only transmits if the channel is verified clear.

```c
void Broadcast_Telemetry(void) {
    uint8_t telemetry_data[] = "LoRa Node 1 Telemetry Check Packet";
    uint32_t max_allowed_timeout_ms = 2000; // Relinquish attempt if medium is locked for > 2 seconds

    // Dispatch the packet utilizing the automated LBT state machine
    LoRa_Status_t tx_result = LoRa_Transmit_LBT(&lora_device, telemetry_data, sizeof(telemetry_data), max_allowed_timeout_ms);

    if (tx_result == LORA_OK) {
        // Packet successfully dispatched! Module automatically reverting back to Continuous RX mode.
    } else if (tx_result == LORA_BUSY) {
        // High ambient congestion; channel remained occupied throughout the 2-second deadline. Aborted.
    } else {
        // Hardware interface exceptions or hard transmission timeout conditions encountered.
    }
}

```

---

## 🛰️ Step 6: Full Application Reference Architecture (Node & Gateway Structure)

The following reference implementation establishes a network topology where a **Sensor Node** collects environmental telemetry metrics and transmits them securely via automated **LBT** to a central **Gateway Receiver Unit**.

### Shared Protocol Struct Definition

Both the Gateway and Node firmware targets must use an identical, packed structure layout to match field offsets seamlessly over the air:

```c
typedef struct __attribute__((packed)) {
    uint8_t  node_id;
    int16_t  temperature;    /* scaled by x10, e.g., 275 = 27.5°C */
    int16_t  humidity_air;   /* scaled by x10, e.g., 600 = 60.0%  */
    int16_t  humidity_soil;  /* scaled by x10, e.g., 450 = 45.0%  */
} Sensor_Data_t;

```

### End-Device Sensor Node Implementation (`test_node.c`)

This target loops every 3000ms, packs mock sensor records, and leverages LBT to access the channel reliably:

```c
#include <string.h>
#include "LoRa.h"

static LoRa_Config_t g_node_lora_cfg;
static uint32_t s_last_tx = 0;

LoRa_Status_t Test_Node_Init(void) {
    mPrint("--- NODE INIT ---\r\n");

    g_node_lora_cfg.nodeId                = 2;   /* Unique device Node ID */
    g_node_lora_cfg.frequency             = 433000000UL;
    g_node_lora_cfg.power                 = 20;
    g_node_lora_cfg.overCurrentProtection = 120;
    g_node_lora_cfg.spredingFactor        = 7;
    g_node_lora_cfg.bandWidth             = BW_125KHz;
    g_node_lora_cfg.crcRate               = CR_4_5;
    g_node_lora_cfg.syncWord              = 0x12;
    g_node_lora_cfg.preamble              = 8;
    g_node_lora_cfg.enableCRC             = 1;
    g_node_lora_cfg.implicitHeader        = 0;
    g_node_lora_cfg.current_mode          = STNBY_MODE;
    g_node_lora_cfg.on_receive            = NULL;
    
    LoRa_Status_t status = LORA_ERROR;
    for (uint8_t retry = 0; retry < 3; retry++) {
        LoRa_Platform_DelayMs(100);
        status = LoRa_Init(&g_node_lora_cfg);
        if (status == LORA_OK) break;
        mPrint("INIT FAIL: %d, retry %d/3...\r\n", (int)status, (int)(retry + 1));
    }

    if (status != LORA_OK) {
        mPrint("INIT FAILED after 3 retries\r\n");
        return status;
    }

    s_last_tx = LoRa_Platform_GetTickMs();
    mPrint("INIT OK\r\n");
    return LORA_OK;
}

void Test_Node_Run(void) {
    if (LoRa_Platform_GetTickMs() - s_last_tx < 3000) return;
    s_last_tx = LoRa_Platform_GetTickMs();

    Sensor_Data_t data = {
        .node_id       = 2,
        .temperature   = 275,   /* 27.5 °C */
        .humidity_air  = 600,   /* 60.0 % */
        .humidity_soil = 450    /* 45.0 % */
    };

    mPrint("Sending data...\r\n");

    LoRa_Status_t tx_status = LoRa_Transmit_LBT(
        &g_node_lora_cfg,
        (uint8_t*)&data,
        sizeof(Sensor_Data_t),
        1000
    );

    switch (tx_status) {
        case LORA_OK:
            mPrint("TX OK\r\n\r\n");
            break;
        case LORA_BUSY:
            mPrint("TX BUSY (channel busy)\r\n\r\n");
            break;
        case LORA_TIMEOUT:
            mPrint("TX TIMEOUT\r\n\r\n");
            break;
        default:
            mPrint("TX FAIL: %d\r\n\r\n", (int)tx_status);
            break;
    }
}

```

### Central Base Gateway Implementation (`test_gateway.c`)

The central coordinator stays in Continuous RX mode, extracts raw data safely using localized FIFO snapshots, parses scaling factors, and exposes link telemetry indexes:

```c
#include <string.h>
#include "LoRa.h"

static LoRa_Config_t g_gateway_lora_cfg;

static void OnRxDone_Callback(uint8_t packet_len) {
    mPrint("[GW] IRQ RxDone, raw_len=%d\r\n", (int)packet_len);
}

void Test_Gateway_IRQHandler(void) {
    LoRa_IRQHandler(&g_gateway_lora_cfg);
}

LoRa_Status_t Test_Gateway_Init(void) {
    mPrint("--- GATEWAY INIT ---\r\n");

    g_gateway_lora_cfg.nodeId                = 0;
    g_gateway_lora_cfg.frequency             = 433000000UL;
    g_gateway_lora_cfg.power                 = 20;
    g_gateway_lora_cfg.overCurrentProtection = 120;
    g_gateway_lora_cfg.spredingFactor        = 7;
    g_gateway_lora_cfg.bandWidth             = BW_125KHz;
    g_gateway_lora_cfg.crcRate               = CR_4_5;
    g_gateway_lora_cfg.syncWord              = 0x12;
    g_gateway_lora_cfg.preamble              = 8;
    g_gateway_lora_cfg.enableCRC             = 1;
    g_gateway_lora_cfg.implicitHeader        = 0;
    g_gateway_lora_cfg.current_mode          = STNBY_MODE;
    g_gateway_lora_cfg.on_receive            = NULL;
    
    LoRa_Status_t status = LORA_ERROR;
    for (uint8_t retry = 0; retry < 3; retry++) {
        LoRa_Platform_DelayMs(100);
        status = LoRa_Init(&g_gateway_lora_cfg);
        if (status == LORA_OK) break;
        mPrint("INIT FAIL: %d, retry %d/3...\r\n", (int)status, (int)(retry + 1));
    }

    if (status != LORA_OK) {
        mPrint("INIT FAILED after 3 retries\r\n");
        return status;
    }

    LoRa_RegisterRxCallback(&g_gateway_lora_cfg, OnRxDone_Callback);
    mPrint("INIT OK. Listening...\r\n");
    LoRa_RxStart(&g_gateway_lora_cfg);
    return LORA_OK;
}

void Test_Gateway_Run(void) {
    uint8_t  rx_buf[255];
    uint8_t  rx_len    = 0;
    uint16_t dummy_seq = 0;

    LoRa_Status_t st = LoRa_ReadPacketData(
        &g_gateway_lora_cfg, rx_buf, sizeof(rx_buf), &rx_len, &dummy_seq);

    if (st != LORA_OK || rx_len == 0) return;

    if (rx_len < sizeof(Sensor_Data_t)) {
        mPrint("[GW] Payload too short: %d (need %d)\r\n", (int)rx_len, (int)sizeof(Sensor_Data_t));
        return;
    }

    Sensor_Data_t data;
    memcpy(&data, rx_buf, sizeof(Sensor_Data_t));

    int16_t t  = data.temperature;
    int16_t ha = data.humidity_air;
    int16_t hs = data.humidity_soil;

    mPrint("=== RECEIVED PACKET ===\r\n");
    mPrint("Node ID   : %d\r\n", (int)data.node_id);

    if (t < 0) {
        mPrint("Temp      : -%d.%d C\r\n", (int)(-t / 10), (int)(-t % 10));
    } else {
        mPrint("Temp      : %d.%d C\r\n",  (int)(t  / 10), (int)(t  % 10));
    }
    mPrint("Air Humid : %d.%d %%\r\n",  (int)(ha / 10), (int)(ha % 10));
    mPrint("Soil Humid: %d.%d %%\r\n",  (int)(hs / 10), (int)(hs % 10));
    mPrint("RSSI: %d dBm | SNR: %d dB\r\n\r\n", LoRa_GetPacketRSSI(), LoRa_GetPacketSNR());
}

```

---

## 📊 Step 7: Diagnostic Network Tracking

The driver maintains and updates a global statistical tracker `g_lora_stats`. You can output these variables to a diagnostic UART console or display to evaluate link stability and data collision rates in real-time:

```c
void Log_Network_Diagnostics(void) {
    printf("Total Packets Transmitted Successfully: %d\n", g_lora_stats.tx_ok);
    printf("Total Packets Aborted (TX Timeout): %d\n", g_lora_stats.tx_fail);
    printf("LBT Collisions Blocked (Medium Busy): %d\n", g_lora_stats.tx_busy);
    printf("Total Packets Extracted Successfully: %d\n", g_lora_stats.rx_ok);
    printf("Dropped Packets (CRC Corruptions): %d\n", g_lora_stats.rx_crc_err);
    printf("Sequence ID of Last Received Frame: %d\n", g_lora_stats.last_seq_rx);
}

```

```

```
