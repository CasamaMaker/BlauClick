#pragma once

#include <stdint.h>

void readAllConfigs();
uint8_t getCachedChannel();
void setCachedChannel(uint8_t ch);

#ifndef HARDCODED_CONFIG
void clearConfig();
void saveMac();
void saveSSID();
void deleteMac();
#endif
