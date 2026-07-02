

# STM32 LoRa (SX1278) P2P Communication Library

A lightweight, reliable, and non-blocking Peer-to-Peer (P2P) communication library for **SX127x/RFM95** LoRa modules using **STM32 HAL**. It features an interrupt-driven double-buffer architecture for the Gateway to prevent packet loss and a low-power polling mechanism for End Nodes.

## 📂 Project Structure

The project consists of 4 core files:

| File Name | Type | Description |
| :--- | :--- | :--- |
| **`LoRa.h`** | Header | Pin definitions, register maps, configuration structures, and core function prototypes. |
| **`LoRa.c`** | Source | Low-level SPI abstraction, hardware register manipulation, and driver implementations. |
| **`Test_Gateway.c`** | Application | Gateway-side application using **EXTI Interrupts** for continuous background listening. |
| **`Test_Node.c`** | Application | Node-side application using **Polling Mode** and automatic Power-Saving Sleep cycles. |

---

## 🚀 Key Features

* **Zero-Packet-Loss Architecture:** Gateway reads FIFO data instantly on EXTI trigger and immediately re-arms RX mode within 60 microseconds.
* **Safe Double-Buffering:** Decouples time-critical hardware SPI reads (Interrupt context) from data processing/UART logging (Main loop context).
* **Listen Before Talk (LBT):** Built-in Channel Activity Detection (CAD) helper to check for preambles before emitting RF power.
* **Low Power Optimization:** Nodes automatically go into deep `SLEEP_MODE` between transmission cycles to conserve energy.

---

## 🛠️ Hardware Hardware Prerequisites

Before compiling, you must implement the following hardware-specific platform functions inside your board support package (typically mapped to STM32 HAL SPI & GPIO):

```c
void     LoRa_Platform_SPI_Init(void);
uint8_t  LoRa_Platform_SPI_Transfer(uint8_t val);
void     LoRa_Platform_NSS_Low(void);
void     LoRa_Platform_NSS_High(void);
void     LoRa_Platform_RESET_Low(void);
void     LoRa_Platform_RESET_High(void);
void     LoRa_Platform_DelayMs(uint32_t ms);
uint32_t LoRa_Platform_GetTickMs(void);
int8_t SHT3x_Platform_I2C_Transmit(uint8_t address, uint8_t* p_data, uint16_t size, uint32_t timeout);
int8_t SHT3x_Platform_I2C_Receive(uint8_t address, uint8_t* buffer, uint16_t size, uint32_t timeout);
void   SHT3x_Platform_Delay_ms(uint32_t ms);

```

---

## 💻 Quick Start

### 1. Gateway Deployment

Call `Test_Gateway_Init()` once during initialization, and poll `Test_Gateway_Run()` inside the main super loop:

```c
int main(void) {
    // ... Hardware Init ...
    Test_Gateway_Init();
    
    while (1) {
        Test_Gateway_Run();
    }
}

```

### 2. Transmitter Node Deployment

Call `Test_Node_Init()` once during initialization, and let `Test_Node_Run()` manage periodic transmissions and sleep states:

```c
int main(void) {
    // ... Hardware Init ...
    Test_Node_Init();
    
    while (1) {
        Test_Node_Run(); // Automatically handles 5-second sleep delays
    }
}

```

---

## 📝 License

This project is open-source and available under the **MIT License**.

```

```
