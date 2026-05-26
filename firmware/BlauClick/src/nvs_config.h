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
void saveCmd1Click(uint8_t cmd, uint8_t p1, uint8_t p2, uint8_t p3);
#endif
