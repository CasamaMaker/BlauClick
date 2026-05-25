#pragma once

#include <Arduino.h>

inline bool isMacValid(const uint8_t* mac) {
  for (int i = 0; i < 6; i++) {
    if (mac[i] != 0xFF) return true;
  }
  return false;
}

inline String macToString(const uint8_t *mac) {
  char buf[18];
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  String s(buf);
  s.toUpperCase();
  return s;
}
