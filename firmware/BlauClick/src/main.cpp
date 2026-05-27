// BlauClick, codi que te el boto que va amb bateria
/* BlauClick projecte
 * https://github.com/CasamaMaker/BlauClick
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

#include "globals.h"
#include "utils.h"
#include "nvs_config.h"
#include "battery.h"
#include "espnow.h"
#include "webserver.h"
#include "wifi_ap.h"


// ── Pins dinàmics de hardware (PIN_UNUSED per defecte; sobreescrits per loadHwGpioConfig()) ─
int g_pinEnVbat  = PIN_UNUSED;
int g_pinVbat    = PIN_UNUSED;
int g_pinBtn     = PIN_UNUSED;
int g_pinBtnInv  = PIN_UNUSED;
int g_pinEnBtn   = PIN_UNUSED;
int g_pinLedDig  = PIN_UNUSED;
int g_pinLed     = PIN_UNUSED;
int8_t g_hwTemplate = -1;

// ── LED ──────────────────────────────────────────────────────────
Adafruit_NeoPixel strip(NUM_LEDS, PIN_UNUSED, NEO_GRB + NEO_KHZ800);

// ── Web server / DNS ─────────────────────────────────────────────
AsyncWebServer server(HTTP_PORT);
DNSServer dnsServer;

// ── MAC target ───────────────────────────────────────────────────
String strMac;
char macStr[18];
byte receiverMac[6];
String receiverSSID = "";

// ── 1-click command ──────────────────────────────────────────────
uint8_t g_cmd1 = 0x01u;  // CMD_TOGGLE per defecte
uint8_t g_p1_1 = 0;
uint8_t g_p2_1 = 0;
uint8_t g_p3_1 = 0;

String myAddresss, myAddresssDoted, myAddresssEnd;

// ── ADC i bateria ────────────────────────────────────────────────
// esp_adc_cal only available on ESP32, S2 — C3/S3/C6 use analogReadMilliVolts()
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2)
#include "esp_adc_cal.h"
esp_adc_cal_characteristics_t adc_chars;
#define USE_ESP_ADC_CAL
#endif
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
//  SETUP
// ════════════════════════════════════════════════════════════════

void setup() {
  startTime = millis();

  Serial.begin(SERIAL_BAUD);

  loadHwGpioConfig();  // sobreescriu g_pin* des de NVS (o manté defaults de compilació)

  if (g_pinBtn    != PIN_UNUSED) pinMode(g_pinBtn,    INPUT_PULLUP);
  if (g_pinBtnInv != PIN_UNUSED) pinMode(g_pinBtnInv, INPUT_PULLDOWN);
  if (g_pinEnBtn  != PIN_UNUSED) { pinMode(g_pinEnBtn, OUTPUT); digitalWrite(g_pinEnBtn, HIGH); }

  Serial.println("[BOOT] inici BlauClick " FIRMWARE_VERSION);

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

  if (g_pinLedDig != PIN_UNUSED) {
    strip.setPin(g_pinLedDig);
    strip.begin();
    strip.setBrightness(map(BRIGHTNESS_DEF, 0, 100, 0, 255));
    strip.clear();
    strip.show();
  }
  if (g_pinLed != PIN_UNUSED) { pinMode(g_pinLed, OUTPUT); digitalWrite(g_pinLed, LOW); }

  #ifndef HARDCODED_CONFIG
  if (!hwConfigIsValid()) {
    Serial.println("[BOOT] Sense config hardware -> mode AP de configuració");
    wifiApModeServer();
    while (1) {
      dnsServer.processNextRequest();
      uint16_t osc   = (millis() / 2) % 510;
      uint8_t  bright = (osc < 255) ? osc : 510 - osc;
      strip.setBrightness(bright);
      strip.setPixelColor(0, COLOR_WIFI_AP);
      strip.show();
    }
  }
  #endif

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
  auto btnIsIdle = [&]() -> bool {
    if (g_pinBtn    != PIN_UNUSED) return  digitalRead(g_pinBtn);    // HIGH = no premut (pull-up)
    if (g_pinBtnInv != PIN_UNUSED) return !digitalRead(g_pinBtnInv); // LOW  = no premut (pull-down)
    return true;
  };

  if (btnIsIdle()) {
    strip.clear();
    strip.show();
    delay(100);
  } else {
    if (g_pinEnBtn != PIN_UNUSED) {
      digitalWrite(g_pinEnBtn, LOW);
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
        if (g_pinEnBtn != PIN_UNUSED) {
          digitalWrite(g_pinEnBtn, LOW);
        } else {
          esp_deep_sleep_start();
        }
      }

      static bool lastIdle = false;
      bool nowIdle = btnIsIdle();

      if (nowIdle && !lastIdle && !buttonReleased) {
        buttonReleased = true;
        Serial.println("[BTN] Boto alliberat");
        delay(200);
      }
      if (!nowIdle && lastIdle && buttonReleased) {
        Serial.println("[BTN] Boto premut despres d'alliberar -> apagant");
        delay(200);
        if (g_pinEnBtn != PIN_UNUSED) {
          digitalWrite(g_pinEnBtn, LOW);
        } else {
          esp_deep_sleep_start();
        }
      }
      lastIdle = nowIdle;
    }
  }
}
