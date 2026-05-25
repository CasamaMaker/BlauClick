#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include "globals.h"
#include "utils.h"
#include "nvs_config.h"


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
  receiverSSID = prefs.getString("ssid", "");
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

void saveMac() {
  prefs.begin("blau", false);
  prefs.putBytes("mac", receiverMac, 6);
  prefs.end();
  Serial.printf("[NVS] MAC guardada: %s\n", strMac.c_str());
}

void saveSSID() {
  prefs.begin("blau", false);
  prefs.putString("ssid", receiverSSID);
  prefs.end();
  Serial.printf("[NVS] SSID guardat: %s\n", receiverSSID.c_str());
}

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
