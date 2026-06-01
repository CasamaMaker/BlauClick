<div align="center">

# BlauClick 🔘

**Botó intel·ligent ESP32-C3 amb bateria per al control ràpid i sense fils de càrregues**

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

*El costat **emissor** de l'ecosistema Blau — envia events de botó sense fils a un receptor [BlauLux](https://github.com/CasamaMaker/BlauLux).*

</div>

---

[🌐 Ecosistema](#-ecosistema-blau) · [✨ Característiques](#-característiques) · [🔌 Maquinari](#-maquinari) · [🚀 Primers passos](#-primers-passos) · [⚙️ Configuració](#️-configuració) · [📖 Ús](#-ús) · [📡 Protocol](#-blauprotocol) · [📁 Estructura](#-estructura-del-projecte) · [🔧 Resolució de problemes](#-resolució-de-problemes) · [🔗 Relacionats](#-projectes-relacionats)

---

## 🌐 Ecosistema Blau

BlauClick és l'**emissor** d'un sistema sense fils complet per controlar llums i càrregues AC — sense router ni hub:

```
┌──────────────────┐    ESP-NOW (IEEE 802.11)   ┌──────────────────────┐
│     BlauClick    │ ─────────────────────────► │       BlauLux        │
│  (emissor botó)  │ ◄──────────── ACK ───────  │  (receptor càrrega)  │
│  Bateria · ESP32 │                            │   ESP32  ·   WiFi    │
└──────────────────┘                            └──────────────────────┘
```

La comunicació és **punt a punt a la capa MAC**, sense router intermedi. La latència és < 10 ms i el consum d'energia és mínim. Un BlauClick pot controlar fins a **4 receptors BlauLux** simultàniament.

---

## ✨ Característiques

- 🔋 **Alimentat per bateria** amb llarga autonomia (~1 any d'ús típic)
- ⚡ **Ultra-ràpid** — comunicació ESP-NOW (< 10 ms de latència)
- ✅ **Fiabilitat** — ACK per ordre + fins a 3 reintents automàtics
- 🔁 **Deduplicació** — descarta paquets duplicats en una finestra de 2 s
- 🌐 **Configuració** — portal captiu web, sense aplicació necessària
- 💾 **Persistència** — configuració guardada a NVS (sobreviu talls de corrent)
- 🔌 Carregador i protecció de bateria **integrats**
- 🖨️ Dissenyat per a una integració fàcil en carcassa imprimible en 3D
- 📡 Envia events de clic simple, doble, triple i pulsació llarga
- 👥 **Multi-objectiu** — fins a 4 receptors BlauLux per botó
- 🖥️ **Plataformes** — ESP32-C3 · ESP32 · ESP32-S3 · ESP32-S2 · ESP32-C6
- 🔧 **Firmware** v1.0 — PlatformIO + framework Arduino

---

## 🔌 Maquinari

### 📋 Plantilles de dispositiu

La interfície web ofereix **plantilles predefinides** per a les plaques compatibles:

| Plantilla | Mapatge GPIO | Notes |
|-----------|-------------|-------|
| `BlauClick V1` | EN_VBAT→4 · VBAT→3 · BTN→5 · LED_DIG→6 | PCB personalitzat rev 1 |
| `BlauClick V2` | EN_VBAT→0 · VBAT→3 · BTN→1 · EN_BTN→4 · LED_DIG→5 | PCB personalitzat rev 2 |
| `PICO Click` | VBAT→4 · BTN→5 · EN_BTN→3 · LED_DIG→6 | Placa proto genèrica |

### 🔧 Funcions GPIO

| Funció | Tipus | Descripció |
|--------|-------|------------|
| `EN_VBAT` | Sortida digital | Activa el divisor de tensió de bateria |
| `VBAT` | Entrada ADC | Mesura de tensió de bateria |
| `BTN` | Entrada digital (pull-up) | Entrada del botó, actiu en LOW |
| `BTN_INV` | Entrada digital (pull-down) | Entrada del botó, actiu en HIGH |
| `EN_BTN` | Sortida digital | Activa el LDO / rail d'alimentació del botó |
| `LED_DIG` | Sortida digital | Línia de dades NeoPixel / WS2812 |
| `LED` | Sortida digital | LED simple encès/apagat |

### 🔌 Connexions

**BlauClick V1:**
```
GPIO 4  →  EN_VBAT (activa divisor de bateria)
GPIO 3  →  VBAT    (ADC — tensió de bateria)
GPIO 5  →  Botó    (pull-up, premut = LOW)
GPIO 6  →  NeoPixel / dades WS2812
```

**BlauClick V2:**
```
GPIO 0  →  EN_VBAT (activa divisor de bateria)
GPIO 3  →  VBAT    (ADC — tensió de bateria)
GPIO 1  →  Botó    (pull-up, premut = LOW)
GPIO 4  →  EN_BTN  (activació LDO)
GPIO 5  →  NeoPixel / dades WS2812
```

**PICO Click:**
```
GPIO 4  →  VBAT    (ADC — tensió de bateria)
GPIO 5  →  Botó    (pull-up, premut = LOW)
GPIO 3  →  EN_BTN  (activació LDO)
GPIO 6  →  NeoPixel / dades WS2812
```

---

## 🚀 Primers passos

### 📦 Requisits

- [PlatformIO](https://platformio.org/) (CLI o extensió de VSCode)
- Cable USB-C
- Placa ESP32-C3 (o compatible — vegeu plantilles)
- Driver USB-UART si cal (CH340, CP210x)

### 💾 Compilar i carregar

1. Clona el repositori:
   ```bash
   git clone https://github.com/CasamaMaker/BlauClick.git
   cd BlauClick/firmware/BlauClick
   ```

2. Compila i carrega el firmware:
   ```bash
   pio run -e esp32c3 -t upload
   ```

3. Carrega el sistema de fitxers (interfície web):
   ```bash
   pio run -e esp32c3 -t uploadfs
   ```

4. Obre el monitor sèrie per verificar l'arrencada:
   ```bash
   pio device monitor -b 115200
   ```

**Entorns disponibles:** `esp32c3` · `esp32` · `esp32s3` · `esp32s2` · `esp32c6`

### 🔑 Configuració inicial

En el primer arrencada (o després d'esborrar la configuració), el dispositiu no té cap MAC objectiu configurada i entra automàticament en mode AP:

1. Encén el BlauClick.
2. Des del teu mòbil o ordinador, connecta't a la xarxa **`BlauClick_XXXX`** (últims 4 caràcters de la MAC).
3. El portal captiu s'obre automàticament — o navega a `http://192.168.4.1`.
4. Selecciona una plantilla de maquinari (o assigna les funcions GPIO manualment).
5. Escaneja els dispositius propers i selecciona la MAC objectiu de BlauLux.
6. Configura l'ordre a enviar en fer clic.
7. Prem **Desa**. El dispositiu reinicia i entra en funcionament normal.

<p align="center">
  <img src="pictures/web-manager.png" width="400">
</p>

<p align="center">
  <img src="pictures/web-manager_click.png" width="400">
  <img src="pictures/web-manager_hardware.png" width="400">
</p>

---

## ⚙️ Configuració

### 🕐 En temps de compilació (`config.h`)

| Macro | Valor per defecte | Descripció |
|-------|------------------|------------|
| `FIRMWARE_VERSION` | `"1.0"` | Cadena de versió del firmware |
| `WIFI_AP_SSID` | `"BlauClick"` | Prefix del nom de l'AP (el sufix MAC s'afegeix automàticament) |
| `WIFI_AP_HOLD_MS` | `3000` | Durada de la pulsació (ms) per entrar en mode AP de configuració |
| `WIFI_AP_TIMEOUT_MS` | `60000` | Temps màxim en mode AP abans de dormir (ms) |
| `BATT_MIN_MV` | `3200` | Tensió mínima de bateria (mV) |
| `BATT_MAX_MV` | `4200` | Tensió màxima de bateria (mV) |
| `NUM_LEDS` | `1` | Nombre de LEDs NeoPixel |
| `BRIGHTNESS_DEF` | `15` | Brillantor de LED per defecte (0–100 %) |
| `ESPNOW_CHANNEL` | `1` | Canal WiFi per a ESP-NOW |

### 🌍 En temps d'execució (interfície web)

Tots els paràmetres de maquinari i comportament es poden canviar des de la interfície web (`http://192.168.4.1`):

- **Plantilla de maquinari** — selecciona una configuració GPIO predefinida
- **Assignació GPIO** — assigna una funció a cada pin
- **MAC objectiu** — adreça ESP-NOW del receptor BlauLux
- **Ordre 1-clic** — ordre i paràmetres a enviar amb una pulsació simple
- **WiFi STA** — connectar a la xarxa de casa (per a OTA o MQTT futur)

---

## 📖 Ús

### 🔘 Accions del botó

| Acció | Resultat |
|-------|----------|
| Pulsació simple | Envia l'ordre configurada (per defecte: `CMD_TOGGLE`) a l'objectiu |
| Pulsació doble | Envia l'event `EVT_CLICK_2` a l'objectiu |
| Pulsació triple | Envia l'event `EVT_CLICK_3` a l'objectiu |
| Pulsació llarga (inici) | Envia l'event `EVT_LONG_START` a l'objectiu |
| Pulsació llarga (allibera) | Envia l'event `EVT_LONG_END` a l'objectiu |
| Mantenir 3+ s | Entra en mode de configuració AP |

La finestra de detecció de clic és de **400 ms** (`BLAU_CLICK_WINDOW_MS`) i el llindar de pulsació llarga és de **800 ms** (`BLAU_LONG_PRESS_MS`).

### 🌐 Interfície web

L'API HTTP és accessible a `http://192.168.4.1` mentre el dispositiu és en mode AP:

| Endpoint | Mètode | Descripció |
|----------|--------|------------|
| `/` | `GET` | Serveix la pàgina web de configuració |
| `/` | `POST` | Desa la MAC objectiu i SSID, després dorm |
| `/mac` | `GET` | Retorna la MAC objectiu actual |
| `/mymac` | `GET` | Retorna l'adreça MAC pròpia del dispositiu |
| `/macList` | `GET` | Escaneja i retorna les xarxes WiFi properes (MAC + SSID) |
| `/deletemac` | `GET` | Esborra la MAC objectiu desada |
| `/battery` | `GET` | Retorna el nivell de bateria i estat de càrrega (JSON) |
| `/info` | `GET` | Retorna la versió de firmware i MAC pròpia (JSON) |
| `/1click_cmd` | `GET` | Retorna l'ordre d'1-clic configurada (JSON) |
| `/save_1click_cmd` | `POST` | Desa una nova ordre d'1-clic (`cmd`, `p1`, `p2`, `p3`) |
| `/hw_gpiomap` | `GET` | Retorna el mapatge actual de funcions GPIO (JSON) |
| `/hw_gpiomap` | `POST` | Desa un nou mapatge GPIO i reinicia |
| `/hw_templates` | `GET` | Retorna totes les plantilles de maquinari predefinides (JSON) |
| `/hw_funclist` | `GET` | Retorna totes les funcions GPIO disponibles (JSON) |
| `/hw_gpiocaps` | `GET` | Retorna les capacitats GPIO per perfil de MCU (JSON) |
| `/hw_clear` | `POST` | Esborra la configuració de maquinari i reinicia |
| `/restart` | `GET` | Reinicia el dispositiu |
| `/clearconfig` | `GET` | Esborra tota la configuració (NVS) i reinicia |
| `/disconnect-ap` | `GET` | Surt del mode AP i posa el dispositiu a dormir |

---

## 📡 BlauProtocol

BlauClick utilitza **BlauProtocol v1** — un protocol binari compacte de **10 bytes** dissenyat per a ESP-NOW:

```
Byte:  0      1      2      3-4        5      6    7    8    9
      [VER | TYPE | SEQ | SRC_ID(2B) | CMD | P1 | P2 | P3 | CRC8]
```

| Camp | Mida | Descripció |
|------|------|------------|
| `VER` | 1 B | Versió del protocol (`0x01`) |
| `TYPE` | 1 B | Tipus de missatge (EVENT, CMD, ACK, PING…) |
| `SEQ` | 1 B | Número de seqüència circular (0–255) per a deduplicació |
| `SRC_ID` | 2 B | Identificador de l'emissor (últims 2 bytes de la MAC) |
| `CMD` | 1 B | Codi d'ordre o event |
| `P1–P3` | 3 B | Paràmetres (brillantor, R/G/B, WW/CW…) |
| `CRC8` | 1 B | CRC-8 (polinomi 0x07) sobre bytes 0–8 |

**Tipus de missatge:** `TYPE_EVENT` · `TYPE_CMD` · `TYPE_ACK` · `TYPE_PING` · `TYPE_PONG` · `TYPE_STATUS_REQ` · `TYPE_STATUS_RSP`

**Events de botó (cmd quan TYPE_EVENT):**

| Event | Codi | Descripció |
|-------|------|------------|
| `EVT_CLICK_1` | `0x11` | Clic simple |
| `EVT_CLICK_2` | `0x12` | Doble clic |
| `EVT_CLICK_3` | `0x13` | Triple clic |
| `EVT_LONG_START` | `0x21` | Inici de pulsació llarga |
| `EVT_LONG_END` | `0x22` | Alliberament de pulsació llarga |

**Ordres directes (cmd quan TYPE_CMD):** `CMD_TOGGLE` · `CMD_ON` · `CMD_OFF` · `CMD_SET_BRIGHTNESS` · `CMD_SET_RGB` · `CMD_SET_CCT` · `CMD_SET_SCENE` · `CMD_DIM_UP` · `CMD_DIM_DOWN`

**Codis ACK:** `ACK_OK` · `ACK_ERROR` · `ACK_DUPLICATE` · `ACK_UNAUTHORIZED` · `ACK_BAD_VERSION` · `ACK_BAD_CRC`

**Constants de temporització:**

| Constant | Valor | Descripció |
|----------|-------|------------|
| `BLAU_ACK_TIMEOUT_MS` | 50 ms | Temps d'espera per intent de reintent |
| `BLAU_MAX_RETRIES` | 3 | Nombre màxim de reintents sense ACK |
| `BLAU_CLICK_WINDOW_MS` | 400 ms | Finestra de detecció de múltiples clics |
| `BLAU_LONG_PRESS_MS` | 800 ms | Llindar de pulsació llarga |
| `BLAU_DEDUP_WINDOW_MS` | 2000 ms | Finestra de deduplicació al receptor |
| `BLAU_MAX_SOURCES` | 8 | Màx. BlauClicks per receptor |
| `BLAU_MAX_TARGETS` | 4 | Màx. receptors per BlauClick |

Especificació completa: [`lib/BlauProtocol/blauprotocol.h`](firmware/BlauClick/lib/BlauProtocol/blauprotocol.h)

---

## 📁 Estructura del projecte

```
BlauClick/
├── src/
│   ├── main.cpp          # Punt d'entrada del firmware, setup, loop
│   ├── config.h          # Plantilles GPIO, perfils MCU, constants
│   ├── globals.h         # Declaracions de variables globals
│   ├── utils.h           # Macros d'utilitat i helpers
│   ├── battery.h/.cpp    # Mesura de tensió de bateria (ADC)
│   ├── nvs_config.h/.cpp # Persistència NVS (Preferences)
│   ├── espnow.h/.cpp     # Inicialització ESP-NOW i callbacks
│   ├── webserver.h/.cpp  # Servidor HTTP i API REST
│   └── wifi_ap.h/.cpp    # Mode AP WiFi i portal captiu
├── lib/
│   └── BlauProtocol/
│       ├── blauprotocol.h        # Estructura de paquets, tipus, constants
│       ├── blauprotocol.cpp      # CRC-8, inicialització de paquets
│       ├── blauprotocol_link.h   # Helpers de l'emissor (construir, enviar, ACK)
│       └── blauprotocol_trg.h    # Helpers del receptor (parsejar, dedup, ACK)
├── data/
│   ├── wifimanager.html   # Interfície web de configuració (i18n via JS)
│   ├── style.css          # Estils de la interfície web
│   └── js/                # Scripts frontend (app, gpio, api, i18n)
└── platformio.ini         # Configuració multi-objectiu de PlatformIO
```

---

## 🔧 Resolució de problemes

| Problema | Causa probable | Solució |
|----------|---------------|---------|
| Sempre entra en mode AP en arrencar | Sense MAC objectiu o plantilla GPIO configurada | Connecta't al portal i desa la configuració |
| El portal captiu no s'obre | DNS del navegador bloquejant | Navega manualment a `http://192.168.4.1` |
| El LED no s'encén | Plantilla GPIO no configurada | Assigna la plantilla correcta a la interfície web |
| No es rep ACK | Canal ESP-NOW incorrecte o receptor apagat | Verifica que `ESPNOW_CHANNEL` coincideixi amb el receptor |
| Configuració no desada | NVS ple o corrupte | Crida `/clearconfig` i torna a configurar |
| Error de compilació | Biblioteca absent | Executa `pio pkg install` per descarregar dependències |
| Port USB no detectat | Driver absent | Instal·la el driver CH340 o CP210x per al teu SO |
| El dispositiu reinicia en prémer el botó | Timeout del watchdog | Revisa el monitor sèrie per veure el motiu del reset |

---

## 🔗 Projectes relacionats

- **[BlauLux](https://github.com/CasamaMaker/BlauLux)** — Controlador receptor de càrrega AC (dimmer, relé, NeoPixel)
- **[BlauClick](https://github.com/CasamaMaker/BlauClick)** — Botó emissor sense fils (complement de BlauLux)

---

## 🤝 Contribuir

1. Fes un fork del repositori
2. Crea una branca:
   ```bash
   git checkout -b feature/la-meva-feature
   ```
3. Confirma els teus canvis
4. Fes push i obre un Pull Request

---

## 📜 Llicència

Llicència MIT. Consulta [`LICENSE.txt`](LICENSE.txt) per a més detalls.

---

## 🙌 Agraïments

Inspirat per:
- [PicoClick-C3](https://github.com/makermoekoe/Picoclick-C3)
- [OBJEX_LINK](https://github.com/salvatoreraccardi/OBJEX_LINK)

---

## 📷 Foto del BlauClick

![BlauClick](pictures/2.jpg)

---

<div align="center">

Fet amb ❤️ per [CasamaMaker](https://github.com/CasamaMaker)

</div>
