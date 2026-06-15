#pragma once

#include <stdint.h>

void loadCmdConfig();
uint8_t getCachedChannel();
void setCachedChannel(uint8_t ch);

// ── BlauProtocol v2 — seguretat (NVS namespace "blau_tx") ──
bool loadSecurityConfig();                   // clau de NVS -> context crypto (al setup)
bool saveSecurityPassword(const char* pwd);  // PBKDF2 -> aes_key + nonce_ctr random inicial
void clearSecurity();                        // esborra clau + nonce -> mode v1
bool securityConfigured();
uint32_t getNextNonce();                     // llegeix nonce_ctr i persisteix n+1 ABANS d'enviar

void clearConfig();
void saveMac();
void saveSSID();
void deleteMac();
void saveCmd1Click(uint8_t cmd, uint8_t p1, uint8_t p2, uint8_t p3);
void loadHwGpioConfig();
void saveHwGpioConfig(uint8_t* funcMap, int8_t tmpl, const char* mcu);
void clearHwGpioConfig();
bool hwConfigIsValid();
