<div align="center">

# 🛡️ IoT-Based Emergency Safety & Health Monitoring System

### *Smart Wearable Device for Women, Elderly, and Patients*

[![ESP32](https://img.shields.io/badge/MCU-ESP32-blue?style=for-the-badge&logo=espressif)](https://www.espressif.com/)
[![Arduino](https://img.shields.io/badge/IDE-Arduino-teal?style=for-the-badge&logo=arduino)](https://www.arduino.cc/)
[![Telegram](https://img.shields.io/badge/Alert-Telegram_Bot-26A5E4?style=for-the-badge&logo=telegram)](https://telegram.org/)
[![IoT](https://img.shields.io/badge/Type-IoT_Wearable-green?style=for-the-badge)](https://github.com/)

A real-time IoT wearable that continuously monitors health parameters and sends instant emergency alerts with live location via Telegram — designed for women, elderly, and patients.

</div>

---

## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Circuit Diagram](#circuit-diagram)
- [System Workflow](#system-workflow)
- [Software & Libraries](#software--libraries)
- [Pin Configuration](#pin-configuration)
- [Setup & Installation](#setup--installation)
- [Telegram Bot Commands](#telegram-bot-commands)
- [How It Works](#how-it-works)
- [Project Structure](#project-structure)
- [Future Enhancements](#future-enhancements)
- [License](#license)

---

## 📌 Overview

This project is an **IoT-based wearable safety device** that bridges the gap between personal safety and healthcare monitoring. The device is compact enough to wear as a wristband or small gadget and provides:

- 🔴 **One-press emergency alerts** sent directly to your guardian's Telegram
- 📊 **Live health monitoring** — heart rate, temperature, humidity
- 📍 **Real-time GPS location sharing** via a Google Maps link
- 💬 **Two-way Telegram communication** for on-demand data requests

> **Use Cases:** Women's safety, elderly care, patient monitoring, seizure/chronic illness response, solo travelers

---

## ✨ Features

| Feature | Description |
|---|---|
| 🆘 Emergency Button | Single press triggers instant Telegram alert with health + GPS data |
| ❤️ Heart Rate Monitor | Continuous BPM tracking with auto-alert on abnormal values |
| 🌡️ Temperature & Humidity | Real-time DHT11 readings with fever threshold alert |
| 📍 GPS Tracking | NEO-6M module provides lat/lng with direct Google Maps link |
| 🔔 Buzzer Alarm | Local audio alert to attract nearby attention |
| 💬 Telegram Bot | Guardian can request `/data` anytime to get live health status |
| ⚠️ Auto Health Alerts | System auto-triggers if heart rate or temperature crosses thresholds |
| 📡 WiFi Connected | ESP32 connects to home/mobile hotspot for cloud communication |

---

## 🔧 Hardware Requirements

| Component | Quantity | Purpose |
|---|---|---|
| ESP32 Dev Kit | 1 | Main microcontroller (WiFi + Processing) |
| Pulse Sensor (KY-039 or similar) | 1 | Heart rate measurement |
| DHT11 Sensor | 1 | Temperature and humidity |
| GPS Module (NEO-6M) | 1 | Real-time location tracking |
| Push Button | 1 | Emergency trigger |
| Active Buzzer (5V) | 1 | Local alarm |
| ON/OFF Toggle Switch | 1 | Power control |
| Li-Po Battery / Power Bank | 1 | Portable power supply |
| Connecting Wires | — | Breadboard/PCB wiring |
| Breadboard or PCB | 1 | Circuit assembly |

**Estimated Cost:** ₹800 – ₹1,200 (approx.)

---

## 🔌 Circuit Diagram

```
                        ┌───────────────────────────────┐
                        │         ESP32 Dev Kit          │
                        │                               │
  Pulse Sensor ─────────┤ GPIO 34 (ADC)                 │
  DHT11 Data   ─────────┤ GPIO 4                        │
  GPS TX       ─────────┤ GPIO 16 (RX2)                 │
  GPS RX       ─────────┤ GPIO 17 (TX2)                 │
  Button       ─────────┤ GPIO 2  (INPUT_PULLUP)        │
  Buzzer (+)   ─────────┤ GPIO 5  (OUTPUT)              │
                        │                               │
  3.3V ────────────────►│ 3.3V → DHT11 VCC              │
  3.3V ────────────────►│ 3.3V → GPS VCC                │
  5V   ────────────────►│ VIN  → Pulse Sensor VCC       │
  GND  ────────────────►│ GND  → All Component GND      │
                        └───────────────────────────────┘
```

> 📷 See `circuit_diagram.png` in this repository for the full visual schematic.

### Component Wiring Summary

| Component | VCC | GND | Signal Pin |
|---|---|---|---|
| Pulse Sensor | 5V (VIN) | GND | GPIO 34 |
| DHT11 | 3.3V | GND | GPIO 4 |
| GPS NEO-6M | 3.3V | GND | TX→GPIO16, RX→GPIO17 |
| Push Button | — | GND | GPIO 2 (Pull-up) |
| Buzzer | GPIO 5 | GND | — |

---

## 🔄 System Workflow

```
┌─────────────────────────────────────────────────────────────────┐
│                        POWER ON                                 │
└──────────────────────────┬──────────────────────────────────────┘
                           │
                           ▼
              ┌────────────────────────┐
              │  ESP32 Initializes     │
              │  WiFi + Sensors + GPS  │
              └────────────┬───────────┘
                           │
          ┌────────────────▼────────────────┐
          │       CONTINUOUS MONITORING      │
          │  ┌──────────┐  ┌─────────────┐  │
          │  │ Pulse    │  │  DHT11      │  │
          │  │ Sensor   │  │ Temp + Hum  │  │
          │  └────┬─────┘  └──────┬──────┘  │
          │       └───────┬───────┘         │
          │               │                 │
          │        ┌──────▼──────┐          │
          │        │  ESP32 MCU  │          │
          │        └──────┬──────┘          │
          └───────────────┼─────────────────┘
                          │
           ┌──────────────┼──────────────┐
           │              │              │
           ▼              ▼              ▼
    ┌─────────────┐ ┌──────────┐ ┌──────────────┐
    │  Emergency  │ │ Abnormal │ │  /data cmd   │
    │   Button    │ │  Health  │ │ via Telegram │
    │  Pressed    │ │ Reading  │ │              │
    └──────┬──────┘ └────┬─────┘ └──────┬───────┘
           └─────────────┴──────────────┘
                          │
                          ▼
              ┌────────────────────────┐
              │  GPS Location Fetched  │
              └────────────┬───────────┘
                           │
                           ▼
          ┌─────────────────────────────────┐
          │  Telegram Alert Sent to Guardian │
          │  ❤️ Heart Rate                   │
          │  🌡️ Temperature                  │
          │  💧 Humidity                     │
          │  📍 GPS Location (Maps Link)     │
          └─────────────────────────────────┘
                           │
                           ▼
              ┌────────────────────────┐
              │   🔔 Buzzer Sounds     │
              │   (Local Alert)        │
              └────────────────────────┘
```

---

## 📦 Software & Libraries

Install the following libraries in Arduino IDE via **Sketch → Include Library → Manage Libraries**:

| Library | Version | Purpose |
|---|---|---|
| `UniversalTelegramBot` | ≥1.3.0 | Telegram Bot communication |
| `ArduinoJson` | ≥6.0.0 | JSON parsing for Telegram |
| `DHT sensor library` | Adafruit | DHT11 temperature & humidity |
| `TinyGPS++` | ≥1.0.2 | GPS NMEA parsing |
| `WiFi` | Built-in | ESP32 WiFi connectivity |

---

## 🛠️ Setup & Installation

### Step 1 — Arduino IDE Setup
1. Install [Arduino IDE 2.x](https://www.arduino.cc/en/software)
2. Add ESP32 board package URL in **File → Preferences**:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Go to **Tools → Board Manager** → Install **esp32 by Espressif Systems** (v2.0.17 recommended)

### Step 2 — Install Libraries
Open Arduino IDE → **Sketch → Include Library → Manage Libraries** and install:
- `UniversalTelegramBot`
- `ArduinoJson`
- `DHT sensor library` (Adafruit)
- `TinyGPS++`

### Step 3 — Create Telegram Bot
1. Open Telegram → Search **@BotFather**
2. Send `/newbot` and follow instructions
3. Copy your **Bot Token**
4. Search **@userinfobot** → send `/start` → copy your **Chat ID**

### Step 4 — Configure the Code
Open `main.ino` and update:
```cpp
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define BOT_TOKEN       "YOUR_TELEGRAM_BOT_TOKEN"
#define CHAT_ID         "YOUR_TELEGRAM_CHAT_ID"
```

### Step 5 — Upload
1. Connect ESP32 via USB
2. Select **Tools → Board → ESP32 Dev Module**
3. Select correct **Port**
4. Click **Upload** ▶️

### Step 6 — Test
- Open Serial Monitor (115200 baud)
- Check WiFi connection
- Press the emergency button
- Watch Telegram for the alert message!

---

## 💬 Telegram Bot Commands

| Command | Response |
|---|---|
| `/start` or `/help` | Shows all available commands |
| `/data` | Returns live heart rate, temp, humidity + GPS location |
| `/status` | Shows device online status, WiFi IP, GPS fix status |
| `/buzzer_off` | Remotely stops the buzzer alarm |
| `/test` | Sends a test alert with current readings |

---

## ⚙️ How It Works

1. **Device Powers ON** → ESP32 connects to WiFi, initializes all sensors, sends "Online" notification to Telegram
2. **Continuous Monitoring** → Pulse sensor reads heart rate every 2 seconds; DHT11 reads temperature and humidity
3. **GPS Acquisition** → NEO-6M module acquires satellite fix (may take 1–2 minutes outdoors)
4. **Emergency Trigger** →
   - User presses emergency button **OR**
   - Heart rate exceeds 120 BPM or drops below 45 BPM **OR**
   - Temperature exceeds 38.5°C
5. **Alert Sent** → Telegram message with all health data + Google Maps link
6. **Buzzer Sounds** → Local audio alarm for 3 seconds
7. **Guardian Queries** → Guardian can send `/data` anytime to get live readings

---

## 📁 Project Structure

```
IoT-Safety-System/
│
├── main.ino                  # Main Arduino sketch (complete code)
├── circuit_diagram.png       # Full circuit schematic
├── workflow_diagram.png      # System workflow diagram
└── README.md                 # This file
```

---

## 🚀 Future Enhancements

- [ ] Add **SpO2 (Blood Oxygen)** sensor for respiratory monitoring
- [ ] **Fall Detection** using MPU6050 accelerometer + gyroscope
- [ ] **Indoor positioning** using WiFi triangulation or BLE beacons
- [ ] **Mobile app** for real-time dashboard and alert history
- [ ] **Geofencing** — auto-alerts when user leaves safe zones
- [ ] **Solar charging** for extended outdoor battery life
- [ ] **ECG monitoring** with AD8232 module
- [ ] **Machine learning** on cloud for predictive health alerts

---

## 📄 License

This project is open-source under the [MIT License](LICENSE).

---

<div align="center">

**Built with ❤️ using ESP32 + Telegram + IoT**

*If this project helped you, please ⭐ star the repo!*

</div>
