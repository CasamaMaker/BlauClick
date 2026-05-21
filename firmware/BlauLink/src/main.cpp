// Blaulink, codi que te el boto que va amb bateria
/* BlauLink projecte
 * https://github.com/CasamaMaker/BlauLink
 */

#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <LittleFS.h>

#include "config.h"

#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include "DNSServer.h"

#include <esp_sleep.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include <esp_now.h>
#include <Adafruit_NeoPixel.h>
#include "blauprotocol.h"
#include "blauprotocol_link.h"
#include "blauprotocol_trg.h"


// ── LED ──────────────────────────────────────────────────────────
Adafruit_NeoPixel strip(NUM_LEDS, PIN_LED, NEO_GRB + NEO_KHZ800);

// ── Web server / DNS ─────────────────────────────────────────────
AsyncWebServer server(HTTP_PORT);
DNSServer dnsServer;

// ── MAC target ───────────────────────────────────────────────────
const char* PARAM_INPUT_1 = "mac";
String strMac;
char macStr[18];
byte receiverMac[6];

String myAddresss, myAddresssDoted, myAddresssEnd;

#define MAX_NETWORKS 5
String macAddresses[MAX_NETWORKS];

// ── Seqüències ESP-NOW ───────────────────────────────────────────
static uint8_t       blau_seq          = 0;
volatile bool        blau_ack_received = false;
volatile uint8_t     blau_ack_seq      = 0;
volatile uint8_t     blau_ack_result   = 0;
static volatile bool _mac_delivery_ok  = false;

// ── ADC i bateria ────────────────────────────────────────────────
#include "esp_adc_cal.h"
esp_adc_cal_characteristics_t adc_chars;
float batteryVoltage;
int   batteryLevel;
bool  isCharging;

// ── Timer ────────────────────────────────────────────────────────
unsigned long startTime;

// ── Preferences (NVS) ───────────────────────────────────────────
#ifndef HARDCODED_CONFIG
Preferences prefs;
#endif


// ════════════════════════════════════════════════════════════════
//  UTILITATS
// ════════════════════════════════════════════════════════════════

bool isMacValid(const uint8_t* mac) {
  for (int i = 0; i < 6; i++) {
    if (mac[i] != 0xFF) return true;
  }
  return false;
}

String macToString(const uint8_t *mac) {
  char buf[18];
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  String s(buf);
  s.toUpperCase();
  return s;
}


// ════════════════════════════════════════════════════════════════
//  GESTIÓ DE CONFIGURACIÓ (NVS / Preferences)
// ════════════════════════════════════════════════════════════════

#ifndef HARDCODED_CONFIG

void clearConfig() {
  prefs.begin("blau", false);
  prefs.clear();
  prefs.end();
  Serial.println("[NVS] Config esborrada");
}

void readAllConfigs() {
  memset(receiverMac, 0xFF, 6);
  prefs.begin("blau", true);
  size_t n = prefs.getBytes("mac", receiverMac, 6);
  prefs.end();

  if (n == 6 && isMacValid(receiverMac)) {
    strMac = macToString(receiverMac);
    Serial.printf("[NVS] MAC llegida: %s\n", strMac.c_str());
  } else {
    memset(receiverMac, 0xFF, 6);
    strMac = "FF:FF:FF:FF:FF:FF";
    Serial.println("[NVS] Cap MAC guardada");
  }
}

void saveMac() {
  prefs.begin("blau", false);
  prefs.putBytes("mac", receiverMac, 6);
  prefs.end();
  Serial.printf("[NVS] MAC guardada: %s\n", strMac.c_str());
}

void deleteMac() {
  memset(receiverMac, 0xFF, 6);
  strMac = "FF:FF:FF:FF:FF:FF";
  prefs.begin("blau", false);
  prefs.remove("mac");
  prefs.remove("ch");
  prefs.end();
  Serial.println("[NVS] MAC i canal esborrats");
}

uint8_t getCachedChannel() {
  prefs.begin("blau", true);
  uint8_t ch = prefs.getUChar("ch", 0);
  prefs.end();
  return (ch >= 1 && ch <= 13) ? ch : 0;
}

void setCachedChannel(uint8_t ch) {
  if (ch >= 1 && ch <= 13) {
    prefs.begin("blau", true);
    uint8_t stored = prefs.getUChar("ch", 0);
    prefs.end();
    if (stored != ch) {
      prefs.begin("blau", false);
      prefs.putUChar("ch", ch);
      prefs.end();
      Serial.printf("[NVS] Canal guardat: %d\n", ch);
    }
  }
}

#else

// Mode HARDCODED: MAC i canal venen de config.h, NVS no s'usa
void readAllConfigs() {
  uint8_t hcMac[] = HC_TARGET_MAC;
  memcpy(receiverMac, hcMac, 6);
  strMac = macToString(receiverMac);
  Serial.printf("[HARDCODED] MAC: %s  canal: %d\n", strMac.c_str(), HC_CHANNEL);
}

uint8_t getCachedChannel() { return HC_CHANNEL; }
void setCachedChannel(uint8_t) {}

#endif


// ════════════════════════════════════════════════════════════════
//  ESP-NOW
// ════════════════════════════════════════════════════════════════

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  _mac_delivery_ok = (status == ESP_NOW_SEND_SUCCESS);
  Serial.println(_mac_delivery_ok ? "[ESPNOW] ACK TX: OK" : "[ESPNOW] ACK TX: FAIL");
}

void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  blau_on_data_recv(mac, data, len, &blau_ack_received, &blau_ack_seq, &blau_ack_result);
}

uint8_t findBlauTriggerChannel() {
  Serial.println("[ESPNOW] Escanejant canal de BlauTrigger...");
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks(false, false, false, 500);
  uint8_t ch = 1;
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i).startsWith("BlauTrigger")) {
      ch = (uint8_t)WiFi.channel(i);
      Serial.printf("[ESPNOW] BlauTrigger trobat '%s' al canal %d\n", WiFi.SSID(i).c_str(), ch);
      break;
    }
  }
  WiFi.scanDelete();
  return ch;
}

void config_ESPNOW(uint8_t channel) {
  WiFi.mode(WIFI_STA);
  Serial.printf("[ESPNOW] Configurant al canal %d\n", channel);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESPNOW] Error initializing");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("[ESPNOW] Failed to add peer");
  }
}

void send_ping() {
  BlauPacket_t pkt;
  blau_seq = millis() & 0xFF;
  blau_build_ping_packet(&pkt, blau_seq);
  blau_ack_received = false;
  esp_err_t r = esp_now_send(receiverMac, (uint8_t*)&pkt, sizeof(pkt));
  Serial.printf("[ESPNOW] PING enviat seq=%d err=0x%X\n", pkt.seq, r);
}

bool send_ESPNOW() {
  BlauPacket_t pkt;
  blau_seq = millis() & 0xFF;
  // blau_build_event_packet(&pkt, blau_seq, EVT_CLICK_1);
  blau_build_cmd_packet(&pkt, blau_seq, CMD_TOGGLE, 0,0,0);
  // blau_build_cmd_packet(&pkt, blau_seq, CMD_SET_RGB, 255,0,0);

  bool ok = blau_send_with_ack(&pkt, receiverMac, &blau_ack_received, &blau_ack_seq, &blau_ack_result);
  blau_seq = millis() & 0xFF;

  strip.setPixelColor(0, ok ? COLOR_ESPNOW_OK : COLOR_ESPNOW_FAIL);
  strip.show();

  if (!ok) Serial.println("[ESPNOW] WARN: sense ACK després de tots els intents");
  return ok;
}


// ════════════════════════════════════════════════════════════════
//  BATERIA
// ════════════════════════════════════════════════════════════════

float getBatteryVoltage() {
  if (PIN_EN_VBAT != PIN_UNUSED) {
    pinMode(PIN_EN_VBAT, OUTPUT);
    digitalWrite(PIN_EN_VBAT, LOW);
  }

  const float voltageDividerRatio = 2.0;
  static esp_adc_cal_characteristics_t adc_chars_local;
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12,
                           ESP_ADC_CAL_VAL_EFUSE_VREF, &adc_chars_local);

  uint32_t sum_mV = 0;
  for (int i = 0; i < BATTERY_SAMPLES; i++) {
    uint32_t raw = analogRead(PIN_VBAT);
    sum_mV += esp_adc_cal_raw_to_voltage(raw, &adc_chars_local);
    delay(2);
  }
  return (sum_mV / BATTERY_SAMPLES * voltageDividerRatio) / 1000.0;
}

int calculateBatteryPercentage(float voltage) {
  const float minV = BATTERY_MIN_MV / 1000.0f;
  const float maxV = BATTERY_MAX_MV / 1000.0f;
  if (voltage >= maxV) return 100;
  if (voltage <= minV) return 0;
  return constrain((int)((voltage - minV) / (maxV - minV) * 100), 0, 100);
}

bool isDeviceCharging() {
  if (PIN_CHARGE == PIN_UNUSED) return false;
  return digitalRead(PIN_CHARGE) == HIGH;
}


// ════════════════════════════════════════════════════════════════
//  XARXES (escaneig per al portal)
// ════════════════════════════════════════════════════════════════

String* scanNetworks() {
  Serial.println("[WIFI] Escanejant xarxes...");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < MAX_NETWORKS; i++) macAddresses[i] = "";
  if (n > 0) {
    for (int i = 0; i < n && i < MAX_NETWORKS; i++) {
      macAddresses[i] = WiFi.BSSIDstr(i) + " >> " + WiFi.SSID(i);
    }
  }
  return macAddresses;
}


// ════════════════════════════════════════════════════════════════
//  SERVIDOR WEB (portal captiu de configuració)
// ════════════════════════════════════════════════════════════════

void serveixWifiManager(AsyncWebServerRequest *request) {
  String path = "/wifimanager_" + String(IDIOMA) + ".html";
  request->send(LittleFS, path, "text/html");
}

void webServerSetup() {
  server.on("/", HTTP_GET, serveixWifiManager);
  server.on("/connecttest.txt", [](AsyncWebServerRequest *request) { request->redirect("http://logout.net"); });
  server.on("/wpad.dat",        [](AsyncWebServerRequest *request) { request->send(404); });
  server.on("/generate_204",    HTTP_GET, serveixWifiManager);
  server.on("/ncsi.txt",        HTTP_GET, serveixWifiManager);
  server.on("/hotspot-detect.html",       HTTP_GET, serveixWifiManager);
  server.on("/library/test/success.html", HTTP_GET, serveixWifiManager);
  server.on("/success.txt",     [](AsyncWebServerRequest *request) { request->send(200); });
  server.on("/redirect",        HTTP_GET, serveixWifiManager);
  server.on("/fwlink",          HTTP_GET, serveixWifiManager);
  server.on("/cdn-cgi/",        HTTP_GET, serveixWifiManager);
  server.on("/canonical.html",  HTTP_GET, serveixWifiManager);
  server.on("/favicon.ico",     [](AsyncWebServerRequest *request) { request->send(404); });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/style.css", "text/css");
    Serial.println("[WEB] Served CSS");
  });

  server.on("/mac", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", strMac);
    Serial.println("[WEB] MAC: " + strMac);
  });

  server.on("/deletemac", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "macDeleted");
    #ifndef HARDCODED_CONFIG
      deleteMac();
    #endif
    Serial.println("[WEB] MAC esborrada");
  });

  server.on("/macList", HTTP_GET, [](AsyncWebServerRequest *request) {
    String macListStr = "";
    String* macList = scanNetworks();
    for (int i = 0; i < MAX_NETWORKS; i++) {
      if (macList[i] != "") macListStr += macList[i] + "\n";
    }
    request->send(200, "text/plain", macListStr);
    Serial.println("[WEB] macList: " + macListStr);
  });

  server.on("/mymac", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", myAddresssDoted);
    Serial.println("[WEB] myMAC: " + myAddresssDoted);
  });

  server.on("/battery", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{\"level\":" + String(batteryLevel) +
                  ",\"charging\":" + String(isCharging ? "true" : "false") + "}";
    request->send(200, "application/json", json);
    Serial.printf("[WEB] battery: %d%% charging=%s\n", batteryLevel, isCharging ? "true" : "false");
  });

  // Retorna "hardcoded" o "web" per al HTML
  server.on("/configMode", HTTP_GET, [](AsyncWebServerRequest *request) {
    #ifdef HARDCODED_CONFIG
      request->send(200, "text/plain", "hardcoded");
    #else
      request->send(200, "text/plain", "web");
    #endif
  });

  // Retorna versió firmware, variant i MAC pròpia
  server.on("/info", HTTP_GET, [](AsyncWebServerRequest *request) {
    #if defined(BLAULINK_V1)
      const char* variant = "V1";
    #elif defined(BLAULINK_V2)
      const char* variant = "V2";
    #elif defined(PICO_CLICK)
      const char* variant = "PICO_CLICK";
    #endif
    String json = "{\"version\":\"" + String(FIRMWARE_VERSION) + "\","
                  "\"variant\":\"" + String(variant) + "\","
                  "\"mac\":\"" + myAddresssDoted + "\"}";
    request->send(200, "application/json", json);
  });

  server.on("/disconnect-ap", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Disconnecting WiFi AP...");
    delay(1000);
    if (PIN_EN_BOTO != PIN_UNUSED) {
      digitalWrite(PIN_EN_BOTO, LOW);
    } else {
      esp_deep_sleep_start();
    }
  });

  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
    #ifndef HARDCODED_CONFIG
      int params = request->params();
      for (int i = 0; i < params; i++) {
        const AsyncWebParameter* p = request->getParam(i);
        if (p->isPost() && p->name() == PARAM_INPUT_1) {
          strMac = p->value().c_str();
          Serial.printf("[WEB] Nova MAC: %s\n", strMac.c_str());
          strMac.replace(":", "");
          if (strMac.length() > 0) {
            for (int j = 0; j < 6; j++) {
              receiverMac[j] = strtol(strMac.substring(j * 2, j * 2 + 2).c_str(), NULL, 16);
            }
            strMac = macToString(receiverMac);
            saveMac();
          }
        }
      }
    #endif
    request->send(200, "text/plain", "Configurat! Ja pots provar");
    delay(1000);
    if (PIN_EN_BOTO != PIN_UNUSED) {
      digitalWrite(PIN_EN_BOTO, LOW);
    } else {
      esp_deep_sleep_start();
    }
  });

  // Pàgines de configuració
  // server.on("/config",   HTTP_GET, [](AsyncWebServerRequest *request) {
  //   request->send(LittleFS, "/config.html",   "text/html");
  // });
  // server.on("/wifi",     HTTP_GET, [](AsyncWebServerRequest *request) {
  //   request->send(LittleFS, "/wifi.html",     "text/html");
  // });
  // server.on("/mqtt",     HTTP_GET, [](AsyncWebServerRequest *request) {
  //   request->send(LittleFS, "/mqtt.html",     "text/html");
  // });
  // server.on("/hardware", HTTP_GET, [](AsyncWebServerRequest *request) {
  //   request->send(LittleFS, "/hardware.html", "text/html");
  // });

  // Accions del dispositiu
  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Reiniciant...");
    delay(500);
    ESP.restart();
  });

  server.on("/clearconfig", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Config esborrada. Reiniciant...");
    #ifndef HARDCODED_CONFIG
      clearConfig();
    #endif
    delay(500);
    ESP.restart();
  });

  server.onNotFound(serveixWifiManager);
  server.begin();
  Serial.println("[WEB] server started");
}


// ════════════════════════════════════════════════════════════════
//  WIFI / ACCESS POINT
// ════════════════════════════════════════════════════════════════

void getMyMacAddress() {
  myAddresssDoted = WiFi.softAPmacAddress();
  myAddresss = myAddresssDoted;
  myAddresss.replace(":", "");
  Serial.printf("[WIFI] MAC AP: %s\n", myAddresssDoted.c_str());
  myAddresssEnd = myAddresss.substring(myAddresss.length() - 4);
}

void wifiApModeServer() {
  WiFi.mode(WIFI_STA);
  getMyMacAddress();
  String fullSSID = String(WIFI_SSID) + "_" + myAddresssEnd;
  WiFi.softAP(fullSSID.c_str(), WIFI_PASSWORD);
  Serial.printf("[WIFI] AP ok: %s  IP: %s\n", fullSSID.c_str(), WiFi.softAPIP().toString().c_str());

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  webServerSetup();
}


// ════════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════════

void setup() {
  startTime = millis();

  pinMode(PIN_BOTO, INPUT);
  if (PIN_EN_BOTO != PIN_UNUSED) {
    pinMode(PIN_EN_BOTO, OUTPUT);
    digitalWrite(PIN_EN_BOTO, HIGH);
  }
  if (PIN_CHARGE != PIN_UNUSED) {
    pinMode(PIN_CHARGE, INPUT);
  }

  Serial.begin(SERIAL_BAUD);
  Serial.println("[BOOT] inici BlauLink " FIRMWARE_VERSION);

  if (!LittleFS.begin()) {
    Serial.println("[BOOT] Error muntant LittleFS");
    return;
  }

  #ifndef HARDCODED_CONFIG
    #ifdef CLEAR_CONFIG
      clearConfig();
      Serial.println("[BOOT] CLEAR_CONFIG: NVS esborrada, reiniciant...");
      ESP.restart();
    #endif
  #endif

  readAllConfigs();

  batteryVoltage = getBatteryVoltage();
  batteryLevel   = calculateBatteryPercentage(batteryVoltage);
  isCharging     = isDeviceCharging();
  Serial.printf("[BATT] %.2fV  %d%%  charging=%s\n",
                batteryVoltage, batteryLevel, isCharging ? "yes" : "no");

  strip.begin();
  strip.setBrightness(map(BRIGHTNESS_DEF, 0, 100, 0, 255));
  strip.clear();
  strip.show();

  if (isMacValid(receiverMac)) {
    uint8_t ch = getCachedChannel();
    if (ch == 0) {
      strip.setPixelColor(0, COLOR_SCAN);
      strip.show();
      ch = findBlauTriggerChannel();
      setCachedChannel(ch);
    } else {
      Serial.printf("[ESPNOW] Usant canal en caché: %d\n", ch);
    }
    config_ESPNOW(ch);
    bool ok = send_ESPNOW();
    if (!ok) {
      strip.setPixelColor(0, COLOR_SCAN);
      strip.show();
      uint8_t newCh = findBlauTriggerChannel();
      setCachedChannel(newCh);
    }
  } else {
    Serial.println("[BOOT] Cap MAC guardada, entrant al mode AP");
    strip.setPixelColor(0, COLOR_NO_MAC);
    strip.show();
  }

  delay(10);
}


// ════════════════════════════════════════════════════════════════
//  LOOP PRINCIPAL
// ════════════════════════════════════════════════════════════════

void loop() {
  if (digitalRead(PIN_BOTO)) {
    strip.clear();
    strip.show();
    delay(100);
  } else {
    if (PIN_EN_BOTO != PIN_UNUSED) {
      digitalWrite(PIN_EN_BOTO, LOW);
    } else {
      esp_deep_sleep_start();
    }
  }

  if (startTime + WIFI_AP_HOLD_MS < millis()) {
    Serial.printf("[BTN] Hold -> mode AP. MAC guardada: %s\n", strMac.c_str());

    wifiApModeServer();

    bool buttonReleased = false;
    while (1) {
      dnsServer.processNextRequest();

      uint16_t osc   = (millis() / 2) % 510;
      uint8_t  bright = (osc < 255) ? osc : 510 - osc;
      strip.setBrightness(bright);
      strip.setPixelColor(0, COLOR_WIFI_AP);
      strip.show();

      if (startTime + WIFI_AP_TIMEOUT_MS < millis()) {
        Serial.println("[AP] Temps excedit");
        delay(200);
        if (PIN_EN_BOTO != PIN_UNUSED) {
          digitalWrite(PIN_EN_BOTO, LOW);
        } else {
          esp_deep_sleep_start();
        }
      }

      static bool lastButtonState = HIGH;
      bool buttonState = digitalRead(PIN_BOTO);

      if (buttonState == LOW && lastButtonState == HIGH && !buttonReleased) {
        buttonReleased = true;
        Serial.println("[BTN] Boto alliberat");
        delay(200);
      }
      if (buttonState == HIGH && lastButtonState == LOW && buttonReleased) {
        Serial.println("[BTN] Boto premut despres d'alliberar -> apagant");
        buttonReleased = false;
        delay(200);
        if (PIN_EN_BOTO != PIN_UNUSED) {
          digitalWrite(PIN_EN_BOTO, LOW);
        } else {
          esp_deep_sleep_start();
        }
      }
      lastButtonState = buttonState;
    }
  }
}
