#pragma once

#include <stdint.h>

void config_ESPNOW(uint8_t channel);
uint8_t findReceiverChannel();
void send_ping();
bool send_ESPNOW();
