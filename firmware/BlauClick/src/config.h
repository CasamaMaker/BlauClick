#pragma once

// ════════════════════════════════════════════════════════════════
//  PINOUT I CONFIGURACIÓ DE HARDWARE
// ════════════════════════════════════════════════════════════════
#define PIN_UNUSED  -1   // pin no connectat / no utilitzat


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


// ════════════════════════════════════════════════════════════════
//  CONFIGURACIÓ HARDWARE DINÀMICA
// ════════════════════════════════════════════════════════════════

enum GpioFunc : uint8_t {
  FUNC_NONE    = 0,   // Sense funció
  FUNC_EN_VBAT = 1,   // Habilita bateria  (Digital OUT)
  FUNC_VBAT    = 2,   // Lectura bateria   (ADC IN)
  FUNC_BTN     = 3,   // Botó pull-up      (Digital IN, actiu LOW)
  FUNC_LED_DIG = 4,   // Led digital       (NeoPixel/WS2812)
  FUNC_EN_BTN  = 5,   // Habilita LDO      (Digital OUT)
  FUNC_BTN_INV = 6,   // Botó pull-down    (Digital IN, actiu HIGH)
  FUNC_LED     = 7,   // Led on/off        (Digital OUT)
};
#define FUNC_COUNT 8

struct GpioFuncEntry {
  GpioFunc    func;
  const char* label;
  bool        isInput;
};

inline const GpioFuncEntry FUNC_LIST[FUNC_COUNT] = {
  { FUNC_NONE,    "Sense funci\xC3\xB3",   false },
  { FUNC_EN_VBAT, "Habilita lectura bateria",       false },
  { FUNC_VBAT,    "Lectura bateria",        true  },
  { FUNC_BTN,     "Bot\xC3\xB3 (pull-up)", true  },
  { FUNC_LED_DIG, "Led digital",            false },
  { FUNC_EN_BTN,  "Habilita LDO/uC",           false },
  { FUNC_BTN_INV, "Bot\xC3\xB3 invertit",  true  },
  { FUNC_LED,     "Led on/off",             false },
};

struct GpioPinTemplate { uint8_t gpio; GpioFunc func; };
struct HwTemplate       { const char* name; GpioPinTemplate pins[6]; uint8_t count; };

inline const HwTemplate HW_TEMPLATES[] = {
  { "BlauClick V1",
    {{ 4, FUNC_EN_VBAT }, { 3, FUNC_VBAT }, { 5, FUNC_BTN }, { 6, FUNC_LED_DIG }},
    4 },
  { "BlauClick V2",
    {{ 0, FUNC_EN_VBAT }, { 3, FUNC_VBAT }, { 1, FUNC_BTN }, { 4, FUNC_EN_BTN }, { 5, FUNC_LED_DIG }},
    5 },
  { "PICO Click",
    {{ 4, FUNC_VBAT }, { 5, FUNC_BTN }, { 3, FUNC_EN_BTN }, { 6, FUNC_LED_DIG }},
    4 },
};
#define HW_TEMPLATE_COUNT 3
