# SX1276 LoRa Driver User Guide (Interrupt & LBT Architecture)

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

## ⚙️ Step 2: Device Configuration & RX Mode Activation

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

## ⚡ Step 3: Configure the DIO0 External Interrupt (Crucial)
The SX1276 transceiver flags critical events (Transmission Done, Reception Done, CRC Corruption) externally via its physical DIO0 hardware pin. You must configure the host MCU GPIO pin connected to DIO0 as an External Interrupt triggered on the Rising Edge.

Forward the raw execution from your MCU's GPIO ISR context straight to the driver's handler:
// Example of your host vi MCU GPIO Interrupt Service Routine (ISR)
void My_MCU_GPIO_Interrupt_Handler(void) {
    // Delegate the processing straight to the LoRa Driver ISR Handler
    LoRa_IRQHandler(&lora_device);
}

## 🔄 Step 4: Processing Packet Data inside the Main Loop
Once the external interrupt fires and validates a frame, the driver automatically resets the hardware pointers and raises the flag g_lora_rx_done = 1. In your non-blocking main while(1) loop or an isolated RTOS task, poll this flag to extract payloads:
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
## 📡 Step 5: Reliable Medium Access (LBT - Listen Before Talk)
To mitigate collisions and data losses in shared, highly congested industrial RF channels, utilize the Listen-Before-Talk (LBT) transmission API. The driver evaluates ambient air activity (CAD); if the medium is occupied, it implements a pseudo-random delay algorithm (Backoff) before retrying. It only transmits if the channel is verified clear.
void Broadcast_Telemetry(void) {
    uint8_t telemetry_data[] = "LoRa Node 1 Telemetry Check Packet";
    uint32_t max_allowed_timeout_ms = 2000; // Relinquish attempt if medium is locked for > 2 seconds

    // Dispatch the packet utilizing the automated LBT state machine
    LoRa_Status_t tx_result = LoRa_Transmit_LBT(&lora_device, telemetry_data, sizeof(telemetry_data), max_allowed_timeout_ms);

    if (tx_result == LORA_OK) {
        // Packet successfully dispatched! Mạch tự động reverting back to Continuous RX mode.
    } else if (tx_result == LORA_BUSY) {
        // High ambient congestion; channel remained occupied throughout the 2-second deadline. Aborted.
    } else {
        // Hardware interface exceptions or hard transmission timeout conditions encountered.
    }
}

## 📊 Step 6: Diagnostic Network Tracking
void Log_Network_Diagnostics(void) {
    printf("Total Packets Transmitted Successfully: %d\n", g_lora_stats.tx_ok);
    printf("Total Packets Aborted (TX Timeout): %d\n", g_lora_stats.tx_fail);
    printf("LBT Collisions Blocked (Medium Busy): %d\n", g_lora_stats.tx_busy);
    printf("Total Packets Extracted Successfully: %d\n", g_lora_stats.rx_ok);
    printf("Dropped Packets (CRC Corruptions): %d\n", g_lora_stats.rx_crc_err);
    printf("Sequence ID of Last Received Frame: %d\n", g_lora_stats.last_seq_rx);
}
