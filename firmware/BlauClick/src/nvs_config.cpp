#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include "globals.h"
#include "utils.h"
#include "nvs_config.h"


// ════════════════════════════════════════════════════════════════
//  GESTIÓ DE CONFIGURACIÓ (NVS / Preferences)
// ════════════════════════════════════════════════════════════════

// Esborra tota la configuració del namespace "blau" a la NVS.
void clearConfig() {
  prefs.begin("blau", false);
  prefs.clear();
  prefs.end();
  Serial.println("[NVS] Config esborrada");
}

// Llegeix MAC, SSID i paràmetres del cmd des de NVS cap a les variables globals.
// Si la MAC no és vàlida, reseteja tot a valors per defecte.
void loadCmdConfig() {

  memset(receiverMac, 0xFF, 6);
  prefs.begin("blau", true);
  size_t n = prefs.getBytes("mac", receiverMac, 6);
  receiverSSID = prefs.getString("ssid", "");
  g_cmd1 = prefs.getUChar("cmd1", 0x01u);
  g_p1_1 = prefs.getUChar("p1_1", 0);
  g_p2_1 = prefs.getUChar("p2_1", 0);
  g_p3_1 = prefs.getUChar("p3_1", 0);
  prefs.end();

  if (n == 6 && isMacValid(receiverMac)) {
    strMac = macToString(receiverMac);
    Serial.printf("[NVS] MAC llegida: %s\n", strMac.c_str());
    if (receiverSSID.length() > 0)
      Serial.printf("[NVS] SSID llegit: %s\n", receiverSSID.c_str());
  } else {
    memset(receiverMac, 0xFF, 6);
    strMac = "FF:FF:FF:FF:FF:FF";
    receiverSSID = "";
    Serial.println("[NVS] Cap MAC guardada");
  }
}

// Persisteix la MAC del receptor (receiverMac) a NVS.
void saveMac() {
  prefs.begin("blau", false);
  prefs.putBytes("mac", receiverMac, 6);
  prefs.end();
  Serial.printf("[NVS] MAC guardada: %s\n", strMac.c_str());
}

// Persisteix l'SSID del receptor (receiverSSID) a NVS.
void saveSSID() {
  prefs.begin("blau", false);
  prefs.putString("ssid", receiverSSID);
  prefs.end();
  Serial.printf("[NVS] SSID guardat: %s\n", receiverSSID.c_str());
}

// Elimina MAC, canal i SSID tant de les variables globals com de NVS.
// S'usa quan es vol desvincular el dispositiu del receptor.
void deleteMac() {
  memset(receiverMac, 0xFF, 6);
  strMac = "FF:FF:FF:FF:FF:FF";
  receiverSSID = "";
  prefs.begin("blau", false);
  prefs.remove("mac");
  prefs.remove("ch");
  prefs.remove("ssid");
  prefs.end();
  Serial.println("[NVS] MAC, canal i SSID esborrats");
}

// Actualitza i persisteix la comanda i paràmetres associats al click simple del botó 1.
void saveCmd1Click(uint8_t cmd, uint8_t p1, uint8_t p2, uint8_t p3) {
  g_cmd1 = cmd; g_p1_1 = p1; g_p2_1 = p2; g_p3_1 = p3;
  prefs.begin("blau", false);
  prefs.putUChar("cmd1", cmd);
  prefs.putUChar("p1_1", p1);
  prefs.putUChar("p2_1", p2);
  prefs.putUChar("p3_1", p3);
  prefs.end();
  Serial.printf("[NVS] CMD1 guardat: cmd=%d p1=%d p2=%d p3=%d\n", cmd, p1, p2, p3);
}

// Retorna el canal Wi-Fi cached a NVS (1–13), o 0 si no n'hi ha cap de vàlid.
uint8_t getCachedChannel() {
  prefs.begin("blau", true);
  uint8_t ch = prefs.getUChar("ch", 0);
  prefs.end();
  return (ch >= 1 && ch <= 13) ? ch : 0;
}

// Guarda el canal a NVS només si ha canviat, per evitar escriptures innecessàries a la flash.
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

// Carrega la configuració de GPIO de hardware des de NVS i omple les variables g_pin*.
// Si la clau "htmpl" no existeix, deixa els pins a PIN_UNUSED.
void loadHwGpioConfig() {

  prefs.begin("blau", true);
  bool hasCfg = prefs.isKey("htmpl");
  if (hasCfg) {
    uint8_t funcMap[47] = {};
    for (int i = 0; i <= 46; i++) {
      char key[6]; snprintf(key, sizeof(key), "hf%d", i);
      funcMap[i] = prefs.getUChar(key, 0);
    }
    uint8_t tmplRaw = prefs.getUChar("htmpl", 255);
    g_hwTemplate = (tmplRaw == 255) ? -1 : (int8_t)tmplRaw;
    String mcuStr = prefs.getString("hmcu", "");
    strncpy(g_hwMcu, mcuStr.c_str(), sizeof(g_hwMcu) - 1);
    g_hwMcu[sizeof(g_hwMcu) - 1] = '\0';
    prefs.end();

    g_pinEnVbat = g_pinVbat = g_pinBtn = g_pinBtnInv = g_pinEnBtn = g_pinLedDig = g_pinLed = PIN_UNUSED;
    for (int i = 0; i <= 46; i++) {
      switch ((GpioFunc)funcMap[i]) {
        case FUNC_EN_VBAT: g_pinEnVbat = i; break;
        case FUNC_VBAT:    g_pinVbat   = i; break;
        case FUNC_BTN:     g_pinBtn    = i; break;
        case FUNC_EN_BTN:  g_pinEnBtn  = i; break;
        case FUNC_LED_DIG: g_pinLedDig = i; break;
        case FUNC_BTN_INV: g_pinBtnInv = i; break;
        case FUNC_LED:     g_pinLed    = i; break;
        default: break;
      }
    }
  } else {
    prefs.end();
  }
  Serial.printf("[HW] LED_DIG=%d LED=%d BTN=%d BTN_INV=%d EN_BTN=%d VBAT=%d EN_VBAT=%d tmpl=%d mcu=%s\n",
                g_pinLedDig, g_pinLed, g_pinBtn, g_pinBtnInv, g_pinEnBtn, g_pinVbat, g_pinEnVbat, g_hwTemplate, g_hwMcu);
}

// Persisteix el mapa de funcions GPIO (pins 0–46), la plantilla de hardware i el MCU a NVS.
void saveHwGpioConfig(uint8_t* funcMap, int8_t tmpl, const char* mcu) {
  prefs.begin("blau", false);
  for (int i = 0; i <= 46; i++) {
    char key[6]; snprintf(key, sizeof(key), "hf%d", i);
    prefs.putUChar(key, funcMap[i]);
  }
  prefs.putUChar("htmpl", (uint8_t)(tmpl < 0 ? 255 : tmpl));
  prefs.putString("hmcu", mcu ? mcu : "");
  prefs.end();
  Serial.println("[HW] Config hardware guardada");
}

// Esborra totes les claus de configuració de GPIO hardware de NVS.
void clearHwGpioConfig() {
  prefs.begin("blau", false);
  for (int i = 0; i <= 46; i++) {
    char key[6]; snprintf(key, sizeof(key), "hf%d", i);
    prefs.remove(key);
  }
  prefs.remove("htmpl");
  prefs.remove("hmcu");
  prefs.end();
  Serial.println("[HW] Config hardware esborrada");
}

// Retorna true si la NVS té una config de hardware vàlida: cal "htmpl" i almenys un pin de botó.
bool hwConfigIsValid() {
  prefs.begin("blau", true);
  if (!prefs.isKey("htmpl")) {
    prefs.end();
    return false;
  }
  bool hasBtn = false;
  for (int i = 0; i <= 46; i++) {
    char key[6];
    snprintf(key, sizeof(key), "hf%d", i);
    uint8_t f = prefs.getUChar(key, 0);
    if (f == (uint8_t)FUNC_BTN || f == (uint8_t)FUNC_BTN_INV) {
      hasBtn = true;
      break;
    }
  }
  prefs.end();
  return hasBtn;
}
