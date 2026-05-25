#include <Arduino.h>
#include <WiFi.h>
#include "DNSServer.h"
#include "config.h"
#include "globals.h"
#include "webserver.h"
#include "wifi_ap.h"


// ════════════════════════════════════════════════════════════════
//  WIFI / ACCESS POINT
// ════════════════════════════════════════════════════════════════

void getMyMacAddress() {
  myAddresssDoted = WiFi.softAPmacAddress();
  myAddresss = myAddresssDoted;
  myAddresss.replace(":", "");
  Serial.printf("[WIFI] MAC AP: %s\n", myAddresssDoted.c_str());
  myAddresssEnd = myAddresss.substring(myAddresss.length() - 4);
}

void wifiApModeServer() {
  // WiFi.mode(WIFI_STA);
  WiFi.mode(WIFI_AP);
  getMyMacAddress();
  String fullSSID = String(WIFI_SSID) + "_" + myAddresssEnd;
  WiFi.softAP(fullSSID.c_str(), WIFI_PASSWORD);
  Serial.printf("[WIFI] AP ok: %s  IP: %s\n", fullSSID.c_str(), WiFi.softAPIP().toString().c_str());

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  webServerSetup();
}
