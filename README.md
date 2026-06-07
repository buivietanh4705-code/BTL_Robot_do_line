# IoT Line Follower Robot: STM32 + ESP8266 (MQTT)

This repository contains the complete firmware and dashboard configuration for an IoT-enabled Line Following Robot. The system utilizes an STM32F401RE for real-time motor control via PID, and an ESP8266 for remote telemetry and control via MQTT and Node-RED.

---

## 1. System Architecture

The project is divided into three main layers:
1.  **Edge / Control Layer (STM32F401RE):** Handles hardware interrupts, ADC/Digital line sensing, and real-time PID motor calculations.
2.  **Gateway Layer (ESP8266):** Acts as a transparent Serial-to-MQTT bridge.
3.  **Application Layer (Node-RED):** Provides a web-based GUI dashboard for tuning PID parameters and monitoring telemetry.

---

## 2. Hardware Pin Mapping

### A. STM32 to L298N Motor Driver
*Timer 1 (TIM1) is utilized for PWM generation with a period of 999.*

| L298N Pin | STM32 Pin | Function |
|:----------:|:---------:|-------------|
| **ENA** | `PA8` *(TIM1_CH1)* | PWM Motor A — **Left Wheel** |
| **IN1** | `PC2` | Direction Motor A |
| **IN2** | `PC3` | Direction Motor A |
| **ENB** | `PA9` *(TIM1_CH2)* | PWM Motor B — **Right Wheel** |
| **IN3** | `PC4` | Direction Motor B |
| **IN4** | `PC5` | Direction Motor B |

### B. STM32 to TCRT5000 Line Sensors (Digital Mode)
*Sensors output `0` on the black line. Software inverts this logic to `1` for weighted error calculation.*

| Position | Weight Multiplier | STM32 Pin | 
|:--------:|:---------:|:-----------:|
| **L2 (Far Left)** | `-2.0` | `PA0` | 
| **L1 (Left)** | `-1.0` | `PA1` |
| **C (Center)** | `0.0` | `PA4` |
| **R1 (Right)** | `+1.0` | `PB0` |
| **R2 (Far Right)**| `+2.0` | `PC1` |

### C. STM32 to ESP8266 (UART Communication)
*Using **USART6** at `115200` baud rate.*

| ESP8266 Pin | STM32 Pin | Function |
|:---------:|:---------:|-------|
| **TX** | `USART6_RX` | ESP8266 TX → STM32 RX |
| **RX** | `USART6_TX` | ESP8266 RX → STM32 TX |
| **GND** | `GND` | Common Ground (Crucial) |

---

## 3. MQTT Protocol & Payload Structure

The system utilizes an MQTT Broker to exchange JSON payloads (You have to change the IP Adress in ESP8266 code).

### 📥 Subscribe: `uet/robot/control`
The ESP8266 listens to this topic and forwards the JSON string directly to the STM32 via Serial.
* **PID Tuning & Base Speed:** `{"Kp":18, "Ki":0, "Kd":10, "Base":230}`
* **Mode Switching:** `{"mode": 1}` (1: Auto/Line Follow, 0: Manual)
* **Manual Direction:** `{"dir": "F"}` (Options: `F`orward, `B`ackward, `L`eft, `R`ight, `S`top)

### 📤 Publish: `uet/robot/telemetry`
The STM32 sends operational data up to the ESP8266 via Serial, which is then published to this topic.
* **Error Reporting:** `{"error": 2.4}` (Used by Node-RED to plot real-time charts).

---

## 4. Node-RED Dashboard Configuration

The provided `flows.json` sets up a responsive web dashboard utilizing `node-red-dashboard`.

**Dashboard Features:**
1.  **PID Tuning Sliders:** Real-time adjustment for $K_p$, $K_i$, $K_d$ (0-300) and Base Speed (0-1000).
2.  **Telemetry Chart:** Live line chart visualizing the current sensor `error` variable for PID tuning.
3.  **Mode Toggle:** Switch between `Auto` (Line Following) and `Manual` control.
4.  **Manual Drive UI:** On-screen D-Pad and invisible keyboard listener allowing WASD control. Hitting any directional key automatically overrides the system into Manual Mode.

> **💡 Setup Instructions:** > 1. Open your Node-RED instance.
> 2. Go to the menu (top right) -> **Import**.
> 3. Paste the contents of `flows.json` and deploy.

---

## 5. Demo

🔗 [System Operation Video Demo](https://drive.google.com/file/d/1dNV2jwhfpyAU3FzxgoF_nkqLMBtWG80H/view?usp=sharing)
