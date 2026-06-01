<div align="center">

# BlauClick 🔘

**Battery-powered ESP32-C3 smart button for fast, wireless load control**

[![GitHub release](https://img.shields.io/github/release/CasamaMaker/BlauClick.svg)](https://github.com/CasamaMaker/BlauClick/releases)
[![GitHub downloads](https://img.shields.io/github/downloads/CasamaMaker/BlauClick/total.svg)](https://github.com/CasamaMaker/BlauClick/releases/latest)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-orange?logo=platformio)](https://platformio.org/)
[![Framework](https://img.shields.io/badge/Framework-Arduino-00979D?logo=arduino)](https://www.arduino.cc/)
[![ESP32-C3](https://img.shields.io/badge/ESP32--C3-RISC--V-blue)](https://www.espressif.com/en/products/socs/esp32-c3)
[![Protocol](https://img.shields.io/badge/Protocol-ESP--NOW-informational)](https://www.espressif.com/en/solutions/low-power-solutions/esp-now)
[![License](https://img.shields.io/github/license/CasamaMaker/BlauClick.svg)](LICENSE.txt)

[English](README.md) |
[Català](README.cat.md) |
[Español](README.es.md)

---

*The **sender** side of the Blau ecosystem — sends button events wirelessly to a [BlauLux](https://github.com/CasamaMaker/BlauLux) receiver.*

</div>

---

[🌐 Ecosystem](#-blau-ecosystem) · [✨ Features](#-features) · [🔌 Hardware](#-hardware) · [🚀 Getting Started](#-getting-started) · [⚙️ Configuration](#️-configuration) · [📖 Usage](#-usage) · [📡 Protocol](#-blauprotocol) · [📁 Structure](#-project-structure) · [🔧 Troubleshooting](#-troubleshooting) · [🔗 Related](#-related-projects)

---

## 🌐 Blau Ecosystem

BlauClick is the **sender** in a complete wireless system for controlling lights and AC loads — no router or hub required:

```
┌──────────────────┐    ESP-NOW (IEEE 802.11)   ┌──────────────────────┐
│     BlauClick    │ ─────────────────────────► │       BlauLux        │
│  (button sender) │ ◄──────────── ACK ───────  │   (load receiver)    │
│  Battery · ESP32 │                            │   ESP32  ·   WiFi    │
└──────────────────┘                            └──────────────────────┘
```

Communication is **peer-to-peer at the MAC layer**, with no router in between. Latency is < 10 ms and power consumption is minimal. One BlauClick can control up to **4 BlauLux** receivers simultaneously.

---

## ✨ Features

- 🔋 **Battery-powered** with long autonomy (~1 year typical usage)
- ⚡ **Ultra-fast** ESP-NOW communication (< 10 ms latency)
- ✅ **Reliability** — ACK per command + up to 3 automatic retries
- 🔁 **Deduplication** — discards duplicate packets within a 2 s window
- 🌐 **Configuration** — captive-portal web UI, no app needed
- 💾 **Persistence** — config saved to NVS (survives power loss)
- 🔌 **Integrated** battery charger and protection
- 🖨️ Designed for easy 3D-printable enclosure integration
- 📡 Sends single click, double click, triple click and long press events
- 👥 **Multi-target** — up to 4 BlauLux receivers per button
- 🖥️ **Platforms** — ESP32-C3 · ESP32 · ESP32-S3 · ESP32-S2 · ESP32-C6
- 🔧 **Firmware** v1.0 — PlatformIO + Arduino framework

---

## 🔌 Hardware

### 📋 Device Templates

The web interface offers **predefined templates** for supported boards:

| Template | GPIO mapping | Notes |
|----------|-------------|-------|
| `BlauClick V1` | EN_VBAT→4 · VBAT→3 · BTN→5 · LED_DIG→6 | Custom PCB rev 1 |
| `BlauClick V2` | EN_VBAT→0 · VBAT→3 · BTN→1 · EN_BTN→4 · LED_DIG→5 | Custom PCB rev 2 |
| `PICO Click` | VBAT→4 · BTN→5 · EN_BTN→3 · LED_DIG→6 | Generic proto board |

### 🔧 GPIO Functions

| Function | Type | Description |
|----------|------|-------------|
| `EN_VBAT` | Digital OUT | Enables battery voltage divider |
| `VBAT` | ADC IN | Battery voltage measurement |
| `BTN` | Digital IN (pull-up) | Button input, active LOW |
| `BTN_INV` | Digital IN (pull-down) | Button input, active HIGH |
| `EN_BTN` | Digital OUT | Enables LDO / button power rail |
| `LED_DIG` | Digital OUT | NeoPixel / WS2812 data line |
| `LED` | Digital OUT | Simple on/off LED |

### 🔌 Connections

**BlauClick V1:**
```
GPIO 4  →  EN_VBAT (enable battery divider)
GPIO 3  →  VBAT    (ADC — battery voltage)
GPIO 5  →  Button  (pull-up, pressed = LOW)
GPIO 6  →  NeoPixel / WS2812 data
```

**BlauClick V2:**
```
GPIO 0  →  EN_VBAT (enable battery divider)
GPIO 3  →  VBAT    (ADC — battery voltage)
GPIO 1  →  Button  (pull-up, pressed = LOW)
GPIO 4  →  EN_BTN  (LDO enable)
GPIO 5  →  NeoPixel / WS2812 data
```

**PICO Click:**
```
GPIO 4  →  VBAT    (ADC — battery voltage)
GPIO 5  →  Button  (pull-up, pressed = LOW)
GPIO 3  →  EN_BTN  (LDO enable)
GPIO 6  →  NeoPixel / WS2812 data
```

---

## 🚀 Getting Started

### 📦 Requirements

- [PlatformIO](https://platformio.org/) (CLI or VSCode extension)
- USB-C cable
- ESP32-C3 board (or compatible — see templates)
- USB-UART driver if needed (CH340, CP210x)

### 💾 Compile and Upload

1. Clone the repository:
   ```bash
   git clone https://github.com/CasamaMaker/BlauClick.git
   cd BlauClick/firmware/BlauClick
   ```

2. Compile and upload the firmware:
   ```bash
   pio run -e esp32c3 -t upload
   ```

3. Upload the filesystem (web UI):
   ```bash
   pio run -e esp32c3 -t uploadfs
   ```

4. Open the serial monitor to verify boot:
   ```bash
   pio device monitor -b 115200
   ```

**Available environments:** `esp32c3` · `esp32` · `esp32s3` · `esp32s2` · `esp32c6`

### 🔑 Initial Configuration

On first boot (or after clearing config), the device has no target MAC configured and automatically enters AP mode:

1. Power on the BlauClick.
2. From your phone or computer, connect to the network **`BlauClick_XXXX`** (last 4 chars of the MAC).
3. The captive portal opens automatically — or navigate to `http://192.168.4.1`.
4. Select a hardware template (or assign GPIO functions manually).
5. Scan for nearby devices and select the target BlauLux MAC address.
6. Configure the command to send on click.
7. Press **Save**. The device restarts and enters normal operation.

<p align="center">
  <img src="pictures/web-manager.png" width="400">
</p>

<p align="center">
  <img src="pictures/web-manager_click.png" width="400">
  <img src="pictures/web-manager_hardware.png" width="400">
</p>

---

## ⚙️ Configuration

### 🕐 Compile-time (`config.h`)

| Macro | Default | Description |
|-------|---------|-------------|
| `FIRMWARE_VERSION` | `"1.0"` | Firmware version string |
| `WIFI_AP_SSID` | `"BlauClick"` | AP name prefix (MAC suffix added automatically) |
| `WIFI_AP_HOLD_MS` | `3000` | Hold duration (ms) to enter config AP mode |
| `WIFI_AP_TIMEOUT_MS` | `60000` | Max time in AP mode before sleeping (ms) |
| `BATT_MIN_MV` | `3200` | Battery minimum voltage (mV) |
| `BATT_MAX_MV` | `4200` | Battery maximum voltage (mV) |
| `NUM_LEDS` | `1` | Number of NeoPixel LEDs |
| `BRIGHTNESS_DEF` | `15` | Default LED brightness (0–100 %) |
| `ESPNOW_CHANNEL` | `1` | WiFi channel for ESP-NOW |

### 🌍 Runtime (Web UI)

All hardware and behaviour parameters can be changed from the web interface (`http://192.168.4.1`):

- **Hardware template** — select a predefined GPIO configuration
- **GPIO assignment** — assign a function to each pin
- **Target MAC** — ESP-NOW address of the BlauLux receiver
- **1-click command** — command and parameters to send on single press
- **WiFi STA** — connect to home network (for OTA or future MQTT)

---

## 📖 Usage

### 🔘 Button Actions

| Action | Result |
|--------|--------|
| Single press | Sends configured command (default: `CMD_TOGGLE`) to target |
| Double press | Sends `EVT_CLICK_2` event to target |
| Triple press | Sends `EVT_CLICK_3` event to target |
| Long press (start) | Sends `EVT_LONG_START` event to target |
| Long press (release) | Sends `EVT_LONG_END` event to target |
| Hold 3+ s | Enters AP configuration mode |

The click detection window is **400 ms** (`BLAU_CLICK_WINDOW_MS`) and the long-press threshold is **800 ms** (`BLAU_LONG_PRESS_MS`).

### 🌐 Web Interface

The HTTP API is accessible at `http://192.168.4.1` while the device is in AP mode:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | `GET` | Serves the configuration web page |
| `/` | `POST` | Saves target MAC and SSID, then sleeps |
| `/mac` | `GET` | Returns the current target MAC |
| `/mymac` | `GET` | Returns this device's own MAC address |
| `/macList` | `GET` | Scans and returns nearby WiFi networks (MAC + SSID) |
| `/deletemac` | `GET` | Clears the saved target MAC |
| `/battery` | `GET` | Returns battery level and charging status (JSON) |
| `/info` | `GET` | Returns firmware version and own MAC (JSON) |
| `/1click_cmd` | `GET` | Returns the configured 1-click command (JSON) |
| `/save_1click_cmd` | `POST` | Saves a new 1-click command (`cmd`, `p1`, `p2`, `p3`) |
| `/hw_gpiomap` | `GET` | Returns current GPIO function mapping (JSON) |
| `/hw_gpiomap` | `POST` | Saves a new GPIO mapping and restarts |
| `/hw_templates` | `GET` | Returns all predefined hardware templates (JSON) |
| `/hw_funclist` | `GET` | Returns all available GPIO functions (JSON) |
| `/hw_gpiocaps` | `GET` | Returns GPIO capabilities per MCU profile (JSON) |
| `/hw_clear` | `POST` | Clears hardware config and restarts |
| `/restart` | `GET` | Restarts the device |
| `/clearconfig` | `GET` | Clears all config (NVS) and restarts |
| `/disconnect-ap` | `GET` | Exits AP mode and puts device to sleep |

---

## 📡 BlauProtocol

BlauClick uses **BlauProtocol v1** — a compact **10-byte** binary protocol designed for ESP-NOW:

```
Byte:  0      1      2      3-4        5      6    7    8    9
      [VER | TYPE | SEQ | SRC_ID(2B) | CMD | P1 | P2 | P3 | CRC8]
```

| Field | Size | Description |
|-------|------|-------------|
| `VER` | 1 B | Protocol version (`0x01`) |
| `TYPE` | 1 B | Message type (EVENT, CMD, ACK, PING…) |
| `SEQ` | 1 B | Circular sequence number (0–255) for deduplication |
| `SRC_ID` | 2 B | Sender identifier (last 2 bytes of MAC) |
| `CMD` | 1 B | Command or event code |
| `P1–P3` | 3 B | Parameters (brightness, R/G/B, WW/CW…) |
| `CRC8` | 1 B | CRC-8 (polynomial 0x07) over bytes 0–8 |

**Message types:** `TYPE_EVENT` · `TYPE_CMD` · `TYPE_ACK` · `TYPE_PING` · `TYPE_PONG` · `TYPE_STATUS_REQ` · `TYPE_STATUS_RSP`

**Button events (cmd when TYPE_EVENT):**

| Event | Code | Description |
|-------|------|-------------|
| `EVT_CLICK_1` | `0x11` | Single click |
| `EVT_CLICK_2` | `0x12` | Double click |
| `EVT_CLICK_3` | `0x13` | Triple click |
| `EVT_LONG_START` | `0x21` | Long press start |
| `EVT_LONG_END` | `0x22` | Long press release |

**Direct commands (cmd when TYPE_CMD):** `CMD_TOGGLE` · `CMD_ON` · `CMD_OFF` · `CMD_SET_BRIGHTNESS` · `CMD_SET_RGB` · `CMD_SET_CCT` · `CMD_SET_SCENE` · `CMD_DIM_UP` · `CMD_DIM_DOWN`

**ACK codes:** `ACK_OK` · `ACK_ERROR` · `ACK_DUPLICATE` · `ACK_UNAUTHORIZED` · `ACK_BAD_VERSION` · `ACK_BAD_CRC`

**Timing constants:**

| Constant | Value | Description |
|----------|-------|-------------|
| `BLAU_ACK_TIMEOUT_MS` | 50 ms | Wait time per retry attempt |
| `BLAU_MAX_RETRIES` | 3 | Maximum retries without ACK |
| `BLAU_CLICK_WINDOW_MS` | 400 ms | Multi-click detection window |
| `BLAU_LONG_PRESS_MS` | 800 ms | Long-press threshold |
| `BLAU_DEDUP_WINDOW_MS` | 2000 ms | Deduplication window on receiver |
| `BLAU_MAX_SOURCES` | 8 | Max BlauClicks per receiver |
| `BLAU_MAX_TARGETS` | 4 | Max receivers per BlauClick |

Full specification: [`lib/BlauProtocol/blauprotocol.h`](firmware/BlauClick/lib/BlauProtocol/blauprotocol.h)

---

## 📁 Project Structure

```
BlauClick/
├── src/
│   ├── main.cpp          # Firmware entry point, setup, loop
│   ├── config.h          # GPIO templates, MCU profiles, constants
│   ├── globals.h         # Global variable declarations
│   ├── utils.h           # Utility macros and helpers
│   ├── battery.h/.cpp    # Battery voltage measurement (ADC)
│   ├── nvs_config.h/.cpp # NVS persistence (Preferences)
│   ├── espnow.h/.cpp     # ESP-NOW init and callbacks
│   ├── webserver.h/.cpp  # HTTP server and REST API
│   └── wifi_ap.h/.cpp    # WiFi AP mode and captive portal
├── lib/
│   └── BlauProtocol/
│       ├── blauprotocol.h        # Packet structure, types, constants
│       ├── blauprotocol.cpp      # CRC-8, packet initialisation
│       ├── blauprotocol_link.h   # Sender helpers (build, send, ACK)
│       └── blauprotocol_trg.h    # Receiver helpers (parse, dedup, ACK)
├── data/
│   ├── wifimanager.html   # Configuration web UI (i18n via JS)
│   ├── style.css          # Web UI styles
│   └── js/                # Frontend scripts (app, gpio, api, i18n)
└── platformio.ini         # PlatformIO multi-target configuration
```

---

## 🔧 Troubleshooting

| Problem | Probable cause | Solution |
|---------|---------------|---------|
| Always enters AP mode on boot | No target MAC or GPIO template configured | Connect to the portal and save the configuration |
| Captive portal does not open | Browser DNS blocking | Navigate manually to `http://192.168.4.1` |
| LED does not light up | GPIO template not set | Assign the correct template in the web UI |
| No ACK received | Wrong ESP-NOW channel or target off | Verify `ESPNOW_CHANNEL` matches the receiver |
| Config not saved | NVS full or corrupt | Call `/clearconfig`, then reconfigure |
| Compilation error | Missing library | Run `pio pkg install` to fetch dependencies |
| USB port not detected | Missing driver | Install CH340 or CP210x driver for your OS |
| Device resets after button press | Watchdog timeout | Check serial monitor for reset reason |

---

## 🔗 Related Projects

- **[BlauLux](https://github.com/CasamaMaker/BlauLux)** — AC load controller receiver (dimmer, relay, NeoPixel)
- **[BlauClick](https://github.com/CasamaMaker/BlauClick)** — Wireless button sender (companion to BlauLux)

---

## 🤝 Contributing

1. Fork the repository
2. Create a branch:
   ```bash
   git checkout -b feature/my-feature
   ```
3. Commit your changes
4. Push and open a Pull Request

---

## 📜 License

MIT License. See [`LICENSE.txt`](LICENSE.txt) for details.

---

## 🙌 Acknowledgements

Inspired by:
- [PicoClick-C3](https://github.com/makermoekoe/Picoclick-C3)
- [OBJEX_LINK](https://github.com/salvatoreraccardi/OBJEX_LINK)

---

## 📷 Picture of BlauClick

![BlauClick](pictures/2.jpg)

---

<div align="center">

Made with ❤️ by [CasamaMaker](https://github.com/CasamaMaker)

</div>
