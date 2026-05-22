# Graph Report - .  (2026-05-18)

## Corpus Check
- 32 files · ~442,791 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 290 nodes · 411 edges · 19 communities detected
- Extraction: 82% EXTRACTED · 18% INFERRED · 0% AMBIGUOUS · INFERRED: 76 edges (avg confidence: 0.84)
- Token cost: 11,145 input · 5,292 output

## Community Hubs (Navigation)
- [[_COMMUNITY_BlauProtocol Specification|BlauProtocol Specification]]
- [[_COMMUNITY_Battery Analyzer v4 (Google Sheets)|Battery Analyzer v4 (Google Sheets)]]
- [[_COMMUNITY_BlauProtocol C++ Implementation|BlauProtocol C++ Implementation]]
- [[_COMMUNITY_Battery Calculator Suite|Battery Calculator Suite]]
- [[_COMMUNITY_BlauLink Firmware Core|BlauLink Firmware Core]]
- [[_COMMUNITY_BlauPacket CRC & Framing|BlauPacket CRC & Framing]]
- [[_COMMUNITY_Firmware Flash Pipeline|Firmware Flash Pipeline]]
- [[_COMMUNITY_LiPo 30mAh Discharge Model|LiPo 30mAh Discharge Model]]
- [[_COMMUNITY_SPIFFS Filesystem Upload|SPIFFS Filesystem Upload]]
- [[_COMMUNITY_Li-ion 27mAh Lifetime Analysis|Li-ion 27mAh Lifetime Analysis]]
- [[_COMMUNITY_ESP32-C3 Flash Tool|ESP32-C3 Flash Tool]]
- [[_COMMUNITY_PCB v1.2 Hardware|PCB v1.2 Hardware]]
- [[_COMMUNITY_LiPo Power Monitoring|LiPo Power Monitoring]]
- [[_COMMUNITY_EspLink Web Manager UI|EspLink Web Manager UI]]
- [[_COMMUNITY_LiPo Discharge Models v2|LiPo Discharge Models v2]]
- [[_COMMUNITY_BlauNode v1 Physical Form|BlauNode v1 Physical Form]]
- [[_COMMUNITY_LiPo Battery Percentage v3|LiPo Battery Percentage v3]]
- [[_COMMUNITY_Battery Voltage Model v1|Battery Voltage Model v1]]
- [[_COMMUNITY_WiFi Manager Route|WiFi Manager Route]]

## God Nodes (most connected - your core abstractions)
1. `BatteryAnalyzer` - 17 edges
2. `BlauProtocol v1 Specification (documents)` - 15 edges
3. `BlauLink / BlauTrigger Coding Guidelines` - 11 edges
4. `setup()` - 10 edges
5. `blau_fill_crc()` - 9 edges
6. `blau_init_packet()` - 9 edges
7. `blau_init_packet() — Zero-init Packet with Version and src_id` - 8 edges
8. `BlauLink Project README` - 8 edges
9. `esptool.py Upload Utility` - 8 edges
10. `Li-ion Battery 3.7V 27mAh` - 8 edges

## Surprising Connections (you probably didn't know these)
- `send_ESPNOW()` --calls--> `blau_build_event_packet()`  [INFERRED]
  firmware\BlauLink\src\main.cpp → firmware\BlauLink\lib\BlauProtocol\blauprotocol_link.h
- `send_ping()` --calls--> `blau_build_ping_packet()`  [INFERRED]
  firmware\BlauLink\src\main.cpp → firmware\BlauLink\lib\BlauProtocol\blauprotocol_link.h
- `BlauProtocol v1 Specification (firmware)` --semantically_similar_to--> `BlauProtocol v1 Specification (documents)`  [EXTRACTED] [semantically similar]
  firmware/BlauLink/BLAUPROTOCOL.md → documents/BLAUPROTOCOL.md
- `Captive Portal Configuration Feature` --implements--> `WiFi Access Point (AP) Mode`  [INFERRED]
  firmware/BlauLink/data/wifimanager_CAT.html → README.md
- `Captive Portal Configuration Feature` --shares_data_with--> `EEPROM Configuration Storage`  [INFERRED]
  firmware/BlauLink/data/wifimanager_CAT.html → documents/BLAUPROTOCOL.md

## Hyperedges (group relationships)
- **BlauProtocol Core Packet Integrity Functions** — blauprotocol_cpp_blau_crc8, blauprotocol_cpp_blau_fill_crc, blauprotocol_cpp_blau_check_crc, blauprotocol_h_BlauPacket_t [EXTRACTED 1.00]
- **BlauLink ESP-NOW Send Data Path** — main_cpp_send_ESPNOW, blauprotocol_link_h_blau_build_cmd_packet, blauprotocol_link_h_blau_send_with_ack, main_cpp_ack_volatile_state [EXTRACTED 1.00]
- **BlauLink ESP-NOW Receive Data Path** — main_cpp_OnDataRecv, blauprotocol_link_h_blau_on_data_recv, blauprotocol_trg_h_blau_parse_packet, main_cpp_ack_volatile_state [EXTRACTED 1.00]
- **BlauTrigger Receive and Response Pipeline** — blauprotocol_trg_h_blau_trg_on_data_recv, blauprotocol_trg_h_blau_is_duplicate, blauprotocol_trg_h_blau_build_ack, blauprotocol_trg_h_blau_action_fn_t, blauprotocol_trg_h_blau_trg_process_pending [EXTRACTED 1.00]
- **Battery Discharge Model Iterative Development (v1 to v4)** — BatteryCalculator_v1_exponential_model, BatteryCalculator_v2_multi_model, BatteryCalculator_v3_lipo_model, BatteryCalculator_BatteryAnalyzer, BatteryCalculator_google_sheets_datasource [INFERRED 0.95]
- **BlauLink WiFi AP Configuration Portal** — main_cpp_wifiApModeServer, main_cpp_webServerSetup, app_flask_app, app_wifi_manager_route, app_mac_route, app_mac_list_route [INFERRED 0.85]
- **BlauProtocol Reliability Mechanisms** — crc8_checksum, seq_number_field, application_ack, deduplication_mechanism, retry_mechanism [EXTRACTED 1.00]
- **BlauLink to BlauTrigger ESP-NOW Communication** — blaulink_device, blautrigger_device, espnow_protocol, blaupacket_struct [EXTRACTED 1.00]
- **Captive Portal Configuration Subsystem** — wifimanager_cat_html, wifimanager_en_html, captive_portal_feature, wifi_ap_mode, mac_management_ui [INFERRED 0.95]
- **Firmware Flash Binary Artifacts** — verbose_upload_txt, firmware_bin, spiffs_partition, platformio_build [EXTRACTED 1.00]

## Communities

### Community 0 - "BlauProtocol Specification"
Cohesion: 0.08
Nodes (42): ACK Must Be Sent from loop() Not Callback, Application-Layer ACK (TYPE_ACK), Battery Level Display in Web UI, BlauLink Device, BlauPacket_t Wire Format (10-byte fixed), BlauProtocol v1 Specification (documents), BlauProtocol v1 Specification (firmware), blauprotocol.h Constants Header (+34 more)

### Community 1 - "Battery Analyzer v4 (Google Sheets)"
Cohesion: 0.08
Nodes (17): BatteryAnalyzer, Genera dades d'exemple per a proves., Converteix voltatge a percentatge de càrrega (SOC) amb corba realista Li-ion., Calcula el SOC basat únicament en el voltatge, sense assumir capacitat., Classe per analitzar la descàrrega de bateries Li-ion., Estima la capacitat real basant-se en:         1. Pendent de descàrrega (mV/dia, Inicialitza l'analitzador de bateries.                  Args:             cap, Calcula paràmetres de la bateria basats en les dades.                  Args: (+9 more)

### Community 2 - "BlauProtocol C++ Implementation"
Cohesion: 0.14
Nodes (23): blau_check_crc(), blau_crc8(), blau_fill_crc(), blau_get_src_id(), blau_init_packet(), blau_build_cmd_packet(), blau_build_event_packet(), blau_build_ping_packet() (+15 more)

### Community 3 - "Battery Calculator Suite"
Cohesion: 0.1
Nodes (25): BatteryAnalyzer Class (v4) — Li-ion Discharge Analysis, Google Sheets Battery Data Source, BatteryCalculator v1 — Exponential Discharge Model, BatteryCalculator v2 — Multi-Model Comparison (Linear/Exp/LiPo), BatteryCalculator v3 — LiPo Realistic Model with Percentage, Flask Web Application (app.py), MAC List REST Endpoint, MAC Address REST Endpoint (+17 more)

### Community 4 - "BlauLink Firmware Core"
Cohesion: 0.16
Nodes (19): blau_send_with_ack(), calculateBatteryPercentage(), config_ESPNOW(), deleteEeprom(), findBlauTriggerChannel(), getBatteryVoltage(), getCachedChannel(), getMyMacAddress() (+11 more)

### Community 5 - "BlauPacket CRC & Framing"
Cohesion: 0.14
Nodes (19): blau_check_crc() — Verify Packet CRC, blau_crc8() — CRC-8 Polynomial 0x07 Implementation, blau_fill_crc() — Fill CRC Field in Packet, blau_get_src_id() — Derive src_id from WiFi MAC, blau_init_packet() — Zero-init Packet with Version and src_id, BlauPacket_t Struct (10-byte binary packet), BlauProtocol Message Type Constants (TYPE_*, EVT_*, CMD_*, ACK_*), blau_build_cmd_packet() — Build TYPE_CMD Packet (+11 more)

### Community 6 - "Firmware Flash Pipeline"
Cohesion: 0.18
Nodes (15): BlauLink Firmware Binary, boot_app0 Binary (0x10000), Bootloader Binary (0x0), ESP32-C3 Chip Revision v0.4, ESP32-C3 (arduino-esp32c3) Target, esptool.py Upload Utility, ESP32-C3 Features: WiFi, BLE, Firmware Binary (main app) (+7 more)

### Community 7 - "LiPo 30mAh Discharge Model"
Cohesion: 0.2
Nodes (14): Battery Capacity 30mAh, Charging Rate 0.04C, Battery Critical Zone (voltage range), Battery Damage Zone (voltage range), LiPo 30mAh Discharge Analysis Chart (Realistic Model), Discharge Velocity 0.38 mA/day, BlauLink Hardware v1.2, LiPo Battery 30mAh (+6 more)

### Community 8 - "SPIFFS Filesystem Upload"
Cohesion: 0.24
Nodes (13): Upload Filesystem Image PlatformIO Task Screenshot, Arduino ESP32 Platform (platform-espressif32), BlauLink PlatformIO Project, Firmware Data Directory, ESP32-C3 Target Hardware, esptool.py Flash Upload Process, PlatformIO Upload Filesystem Image Task, Serial Port COM4 (+5 more)

### Community 9 - "Li-ion 27mAh Lifetime Analysis"
Cohesion: 0.27
Nodes (11): Adjusted Voltage Prediction Curve, Battery Capacity 27mAh, Battery Lifetime 692 Days to 0%, Cutoff Voltage 3.0V, Battery Discharge Curve Chart, Discharge Rate 0.04mA/day, Li-ion Battery 3.7V 27mAh, Nominal Voltage 3.7V (+3 more)

### Community 10 - "ESP32-C3 Flash Tool"
Cohesion: 0.33
Nodes (10): BlauLink Firmware Project, boot_app0.bin (0xe000), bootloader.bin (0x0000), ESP32-C3 Microcontroller, firmware.bin (0x10000), ESP32C3 Flash Download Tool V3.9.7, partitions.bin (0x8000), PlatformIO arduino-esp32c3 Build Output (+2 more)

### Community 11 - "PCB v1.2 Hardware"
Cohesion: 0.27
Nodes (10): BlauLink PCB v1.2, Circular PCB Form Factor, Black Cylindrical Enclosure Cap, ESP32-C3-MINI-1 Module, Espressif Systems, Gold-Plated Edge / Test Pads, SMD Passive Components, Tactile Push Button (+2 more)

### Community 12 - "LiPo Power Monitoring"
Cohesion: 0.43
Nodes (8): Battery State: 80% charge, 4.045V, 05/07/2025, LiPo 30mAh Discharge Analysis Chart (Realistic Model), Discharge Objective: min 0.219mA, BlauLink Hardware v1.2, Battery Life Prediction (~82 days useful, ~79 days total), LiPo Battery 30mAh, Realistic Discharge Model (R²=130.226), Battery Voltage Zones (Full/Normal/Low/Critical)

### Community 13 - "EspLink Web Manager UI"
Cohesion: 0.43
Nodes (8): Found Devices Section (Dispositius trobats), EspLink Manager Web UI, ESP-NOW Peer MAC Management, GitHub Repository Link, Save MAC Button (Guardar MAC), MAC Address Display (Own Device), New MAC Address Input Field, Saved Slave MAC Display

### Community 14 - "LiPo Discharge Models v2"
Cohesion: 0.29
Nodes (6): lipo_discharge_model(), lipo_exponential(), lipo_simple_linear(), Model lineal simple per a LiPo, Model exponencial per a LiPo, Model específic per a bateries LiPo:     - Fase 1: Descàrrega quasi-lineal (car

### Community 15 - "BlauNode v1 Physical Form"
Cohesion: 0.33
Nodes (7): 3D Printed Circular Enclosure, BlauNode v1 PCB, CasaroMaker Logo / Brand, LiPo Battery, Microcontroller IC, Tactile Push Button, USB-C Connector

### Community 17 - "LiPo Battery Percentage v3"
Cohesion: 0.4
Nodes (4): calculate_battery_percentage(), lipo_discharge_model(), Calcula el percentatge de bateria restant segons el temps d'ús, Model específic per a bateries LiPo

### Community 18 - "Battery Voltage Model v1"
Cohesion: 0.67
Nodes (2): Model de voltatge amb descàrrega suau i després sobtada., voltage_model()

### Community 20 - "WiFi Manager Route"
Cohesion: 1.0
Nodes (1): WiFi Manager Web Route

## Knowledge Gaps
- **69 isolated node(s):** `Model de voltatge amb descàrrega suau i després sobtada.`, `Model específic per a bateries LiPo:     - Fase 1: Descàrrega quasi-lineal (car`, `Model lineal simple per a LiPo`, `Model exponencial per a LiPo`, `Calcula el percentatge de bateria restant segons el temps d'ús` (+64 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **Thin community `Battery Voltage Model v1`** (3 nodes): `BatteryCalculator.py`, `Model de voltatge amb descàrrega suau i després sobtada.`, `voltage_model()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `WiFi Manager Route`** (1 nodes): `WiFi Manager Web Route`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `send_ESPNOW() — Send BlauProtocol Packet via ESP-NOW` connect `Battery Calculator Suite` to `BlauPacket CRC & Framing`?**
  _High betweenness centrality (0.010) - this node is a cross-community bridge._
- **Are the 7 inferred relationships involving `blau_fill_crc()` (e.g. with `blau_build_event_packet()` and `blau_build_cmd_packet()`) actually correct?**
  _`blau_fill_crc()` has 7 INFERRED edges - model-reasoned connections that need verification._
- **What connects `Model de voltatge amb descàrrega suau i després sobtada.`, `Model específic per a bateries LiPo:     - Fase 1: Descàrrega quasi-lineal (car`, `Model lineal simple per a LiPo` to the rest of the system?**
  _69 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `BlauProtocol Specification` be split into smaller, more focused modules?**
  _Cohesion score 0.08 - nodes in this community are weakly interconnected._
- **Should `Battery Analyzer v4 (Google Sheets)` be split into smaller, more focused modules?**
  _Cohesion score 0.08 - nodes in this community are weakly interconnected._
- **Should `BlauProtocol C++ Implementation` be split into smaller, more focused modules?**
  _Cohesion score 0.14 - nodes in this community are weakly interconnected._
- **Should `Battery Calculator Suite` be split into smaller, more focused modules?**
  _Cohesion score 0.1 - nodes in this community are weakly interconnected._