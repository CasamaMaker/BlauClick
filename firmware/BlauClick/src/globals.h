#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "ESPAsyncWebServer.h"
#include "DNSServer.h"
#include <Preferences.h>
#include "config.h"

// ADC calibration — legacy driver only on ESP32 original i S2
// C3/S3/C6 usen analogReadMilliVolts() (driver_ng), que és incompatible amb esp_adc_cal
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2)
#include "esp_adc_cal.h"
#define USE_ESP_ADC_CAL
#endif

// ── LED ──────────────────────────────────────────────────────────
extern Adafruit_NeoPixel strip;

// ── Web server / DNS ─────────────────────────────────────────────
extern AsyncWebServer server;
extern DNSServer dnsServer;

// ── MAC target ───────────────────────────────────────────────────
extern String strMac;
extern char   macStr[18];
extern byte   receiverMac[6];
extern String receiverSSID;

extern String myAddresss, myAddresssDoted, myAddresssEnd;

// ── 1-click command ──────────────────────────────────────────────
extern uint8_t g_cmd1;
extern uint8_t g_p1_1;
extern uint8_t g_p2_1;
extern uint8_t g_p3_1;

// ── Bateria ───────────────────────────────────────────────────────
extern float batteryVoltage;
extern int   batteryLevel;
extern bool  isCharging;

// ── Preferences (NVS) ───────────────────────────────────────────
#ifndef HARDCODED_CONFIG
extern Preferences prefs;
#endif

// ── Pins dinàmics de hardware ────────────────────────────────────
extern int g_pinEnVbat;
extern int g_pinVbat;
extern int g_pinBtn;
extern int g_pinBtnInv;
extern int g_pinEnBtn;
extern int g_pinLedDig;
extern int g_pinLed;
extern int8_t g_hwTemplate;
