#pragma once

// ════════════════════════════════════════════════════════════════
//  SELECCIÓ DEL DISPOSITIU  (descomenta un sol)
// ════════════════════════════════════════════════════════════════
// #define BLAUCLICK_V1
#define BLAUCLICK_V2
// #define PICO_CLICK


// ════════════════════════════════════════════════════════════════
//  PINOUT I CONFIGURACIÓ DE HARDWARE
// ════════════════════════════════════════════════════════════════
#define PIN_UNUSED  -1   // pin no connectat / no utilitzat

#if defined(BLAUCLICK_V1)
  #define PIN_EN_VBAT  4
  #define PIN_VBAT     3
  #define PIN_BOTO     5
  #define PIN_EN_BOTO  PIN_UNUSED   // V1 usa deep sleep en lloc d'LDO
  #define PIN_LED      6
  #define PIN_CHARGE   PIN_UNUSED

#elif defined(BLAUCLICK_V2)
  #define PIN_EN_VBAT  0
  #define PIN_VBAT     3
  #define PIN_BOTO     1
  #define PIN_EN_BOTO  4
  #define PIN_LED      5
  #define PIN_CHARGE   PIN_UNUSED

#elif defined(PICO_CLICK)
  #define PIN_EN_VBAT  PIN_UNUSED   // No implementat
  #define PIN_VBAT     4
  #define PIN_BOTO     5
  #define PIN_EN_BOTO  3
  #define PIN_LED      6
  #define PIN_CHARGE   PIN_UNUSED

#else
  #error "Defineix una versió del dispositiu a config.h (BLAUCLICK_V1, BLAUCLICK_V2 o PICO_CLICK)"
#endif


// ════════════════════════════════════════════════════════════════
//  MODE DE CONFIGURACIÓ
//  · Comentat    → configuració via web (MAC i canal guardats a NVS)
//  · Descomentat → MAC i canal fixats al codi; la web UI és de
//                  només lectura i no modifica la configuració
// ════════════════════════════════════════════════════════════════
// #define HARDCODED_CONFIG   // comenta per desactivar

#ifdef HARDCODED_CONFIG
  #define HC_TARGET_MAC  { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }  // MAC del BlauTrigger
  #define HC_CHANNEL     1                                         // Canal Wi-Fi
#endif


// ════════════════════════════════════════════════════════════════
//  ESBORRA CONFIG  (descomenta per esborrar les Preferences de NVS)
//  · Comentat    → comportament normal
//  · Descomentat → esborra tota la config guardada a l'inici
//                  (torna a comentar i repuja el firmware després)
// ════════════════════════════════════════════════════════════════
// #define CLEAR_CONFIG       // comenta per desactivar


// ════════════════════════════════════════════════════════════════
//  WIFI  (Access Point del portal de configuració)
// ════════════════════════════════════════════════════════════════
#define WIFI_SSID      "BlauClick"
#define WIFI_PASSWORD  ""

// ════════════════════════════════════════════════════════════════
//  LED DIGITAL (NeoPixel / WS2812)
// ════════════════════════════════════════════════════════════════
#define NUM_LEDS        1
#define BRIGHTNESS_DEF  15    // brillantor per defecte (0–100)

//  Paleta de colors  — format 0xRRGGBB
//  Escriu el nom directament a les assignacions de sota
#define vermell   0xFF0000
#define verd      0x00FF00
#define blau      0x0000FF
#define groc      0xFFFF00
#define taronja   0xFF8000
#define blanc     0xFFFFFF
#define negre     0x000000
#define lila      0x800080
#define rosa      0xFF00FF
#define cian      0x00FFFF

//  Colors de cada acció
#define COLOR_SCAN         taronja
#define COLOR_ESPNOW_OK    verd
#define COLOR_ESPNOW_FAIL  vermell
#define COLOR_NO_MAC       groc
#define COLOR_WIFI_AP      lila


// ════════════════════════════════════════════════════════════════
//  BATERIA (LiPo 1S)
// ════════════════════════════════════════════════════════════════
#define BATTERY_MIN_MV   3200   // tensió mínima (mV)
#define BATTERY_MAX_MV   4200   // tensió màxima (mV)
#define BATTERY_SAMPLES  10     // mostres ADC per càlcul de la mitja


// ════════════════════════════════════════════════════════════════
//  SISTEMA
// ════════════════════════════════════════════════════════════════
#define FIRMWARE_VERSION    "0.9"
#define SERIAL_BAUD         115200
#define WIFI_AP_HOLD_MS       3000   // ms prement el botó per entrar al mode AP
#define WIFI_AP_TIMEOUT_MS   60000   // ms màxims en mode AP abans d'apagar el dispositiu
#define HTTP_PORT               80
#define DNS_PORT                53
