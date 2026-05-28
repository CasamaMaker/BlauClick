#pragma once

// ════════════════════════════════════════════════════════════════
//  PINOUT I CONFIGURACIÓ DE HARDWARE
// ════════════════════════════════════════════════════════════════
#define PIN_UNUSED  -1   // pin no connectat / no utilitzat



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
  { FUNC_NONE,    "func_none",    false },
  { FUNC_EN_VBAT, "func_en_vbat", false },
  { FUNC_VBAT,    "func_vbat",    true  },
  { FUNC_BTN,     "func_btn",     true  },
  { FUNC_LED_DIG, "func_led_dig", false },
  { FUNC_EN_BTN,  "func_en_btn",  false },
  { FUNC_BTN_INV, "func_btn_inv", true  },
  { FUNC_LED,     "func_led",     false },
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
#define HW_TMPL_CUSTOM    99

// ════════════════════════════════════════════════════════════════
//  PERFILS DE GPIO PER MICROCONTROLADOR
// ════════════════════════════════════════════════════════════════

struct GpioCaps { bool valid; bool hasPwm; bool hasAdc; bool inputOnly; };

static const GpioCaps ESP32C3_GPIO_CAPS[22] = {
  //        valid,  hasPwm,  hasAdc, inputOnly
  {  true,  true,   true,   false },  //  0
  {  true,  true,   true,   false },  //  1
  {  true,  true,   true,   false },  //  2
  {  true,  true,   true,   false },  //  3
  {  true,  true,   true,   false },  //  4
  {  true,  true,  false,   false },  //  5
  {  true,  true,  false,   false },  //  6
  {  true,  true,  false,   false },  //  7
  {  true,  true,  false,   false },  //  8
  {  true,  true,  false,   false },  //  9
  {  true,  true,  false,   false },  // 10
  {  true,  true,  false,   false },  // 11 (flash CS, amb precaucio)
  {  true,  true,  false,   false },  // 12 (flash CLK, amb precaucio)
  {  true,  true,  false,   false },  // 13 (flash DIO, amb precaucio)
  {  true,  true,  false,   false },  // 14 (flash DIO, amb precaucio)
  {  true,  true,  false,   false },  // 15 (flash DIO, amb precaucio)
  {  true,  true,  false,   false },  // 16 (flash DIO, amb precaucio)
  {  true,  true,  false,   false },  // 17 (flash DIO, amb precaucio)
  {  true, false,  false,   false },  // 18 (USB D-)
  {  true, false,  false,   false },  // 19 (USB D+)
  {  true,  true,  false,   false },  // 20
  {  true,  true,  false,   false },  // 21
};

static const GpioCaps ESP32S3_GPIO_CAPS[47] = {
  // valid, hasPwm, hasAdc, inputOnly
  {  true,  true,  false,   false },  //  0 (strapping)
  {  true,  true,   true,   false },  //  1
  {  true,  true,   true,   false },  //  2
  {  true,  true,   true,   false },  //  3
  {  true,  true,   true,   false },  //  4
  {  true,  true,   true,   false },  //  5
  {  true,  true,   true,   false },  //  6
  {  true,  true,   true,   false },  //  7
  {  true,  true,   true,   false },  //  8
  {  true,  true,   true,   false },  //  9
  {  true,  true,   true,   false },  // 10
  {  true,  true,  false,   false },  // 11 (PSRAM on alguns moduls)
  {  true,  true,  false,   false },  // 12 (PSRAM on alguns moduls)
  {  true,  true,  false,   false },  // 13 (PSRAM on alguns moduls)
  {  true,  true,  false,   false },  // 14 (PSRAM on alguns moduls)
  {  true,  true,  false,   false },  // 15 (PSRAM on alguns moduls)
  {  true,  true,  false,   false },  // 16 (PSRAM on alguns moduls)
  {  true,  true,  false,   false },  // 17 (PSRAM on alguns moduls)
  {  true, false,  false,   false },  // 18 (USB D-)
  {  true, false,  false,   false },  // 19 (USB D+)
  {  true,  true,  false,   false },  // 20
  {  true,  true,  false,   false },  // 21
  {  true,  true,  false,   false },  // 22
  {  true,  true,  false,   false },  // 23
  {  true,  true,  false,   false },  // 24
  {  true,  true,  false,   false },  // 25
  {  true,  true,  false,   false },  // 26
  {  true,  true,  false,   false },  // 27
  {  true,  true,  false,   false },  // 28
  {  true,  true,  false,   false },  // 29
  {  true,  true,  false,   false },  // 30
  {  true,  true,  false,   false },  // 31
  {  true,  true,  false,   false },  // 32
  {  true,  true,  false,   false },  // 33
  {  true,  true,  false,   false },  // 34 (flash CS, amb precaucio)
  {  true,  true,  false,   false },  // 35 (flash CLK, amb precaucio)
  {  true,  true,  false,   false },  // 36 (flash DIO, amb precaucio)
  {  true,  true,  false,   false },  // 37 (flash DIO, amb precaucio)
  {  true,  true,  false,   false },  // 38
  {  true,  true,  false,   false },  // 39
  {  true,  true,  false,   false },  // 40
  {  true,  true,  false,   false },  // 41
  {  true,  true,  false,   false },  // 42
  {  true,  true,  false,   false },  // 43 (UART0_TX)
  {  true,  true,  false,   false },  // 44 (UART0_RX)
  {  true,  true,  false,   false },  // 45 (strapping)
  {  true, false,  false,    true },  // 46 (input only)
};

struct McuProfile { const char* id; const char* name; const GpioCaps* caps; uint8_t count; };
inline const McuProfile MCU_PROFILES[] = {
  { "esp32c3", "ESP32-C3", ESP32C3_GPIO_CAPS, 22 },
  { "esp32s3", "ESP32-S3", ESP32S3_GPIO_CAPS, 47 },
};
#define MCU_PROFILE_COUNT 2
