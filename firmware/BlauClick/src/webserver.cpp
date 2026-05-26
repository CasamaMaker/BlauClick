#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <LittleFS.h>
#include <esp_sleep.h>
#include "ESPAsyncWebServer.h"
#include "config.h"
#include "globals.h"
#include "utils.h"
#include "nvs_config.h"
#include "webserver.h"

#define MAX_NETWORKS 5
static String macAddresses[MAX_NETWORKS];
static const char* PARAM_INPUT_1 = "mac";


// ════════════════════════════════════════════════════════════════
//  XARXES (escaneig per al portal)
// ════════════════════════════════════════════════════════════════

String* scanNetworks() {
  Serial.println("[WIFI] Escanejant xarxes...");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < MAX_NETWORKS; i++) macAddresses[i] = "";
  if (n > 0) {
    for (int i = 0; i < n && i < MAX_NETWORKS; i++) {
      macAddresses[i] = WiFi.BSSIDstr(i) + " >> " + WiFi.SSID(i);
    }
  }
  return macAddresses;
}


// ════════════════════════════════════════════════════════════════
//  SERVIDOR WEB (portal captiu de configuració)
// ════════════════════════════════════════════════════════════════

void serveixWifiManager(AsyncWebServerRequest *request) {
  request->send(LittleFS, "/wifimanager.html", "text/html");
}

void webServerSetup() {
  server.on("/", HTTP_GET, serveixWifiManager);
  server.on("/connecttest.txt", [](AsyncWebServerRequest *request) { request->redirect("http://logout.net"); });
  server.on("/wpad.dat",        [](AsyncWebServerRequest *request) { request->send(404); });
  server.on("/generate_204",    HTTP_GET, serveixWifiManager);
  server.on("/ncsi.txt",        HTTP_GET, serveixWifiManager);
  server.on("/hotspot-detect.html",       HTTP_GET, serveixWifiManager);
  server.on("/library/test/success.html", HTTP_GET, serveixWifiManager);
  server.on("/success.txt",     [](AsyncWebServerRequest *request) { request->send(200); });
  server.on("/redirect",        HTTP_GET, serveixWifiManager);
  server.on("/fwlink",          HTTP_GET, serveixWifiManager);
  server.on("/cdn-cgi/",        HTTP_GET, serveixWifiManager);
  server.on("/canonical.html",  HTTP_GET, serveixWifiManager);
  server.on("/favicon.ico",     [](AsyncWebServerRequest *request) { request->send(404); });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/style.css", "text/css");
    Serial.println("[WEB] Served CSS");
  });

  server.serveStatic("/js/", LittleFS, "/js/");

  server.on("/mac", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", strMac);
    Serial.println("[WEB] MAC: " + strMac);
  });

  server.on("/deletemac", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "macDeleted");
    #ifndef HARDCODED_CONFIG
      deleteMac();
    #endif
    Serial.println("[WEB] MAC esborrada");
  });

  server.on("/macList", HTTP_GET, [](AsyncWebServerRequest *request) {
    String macListStr = "";
    String* macList = scanNetworks();
    for (int i = 0; i < MAX_NETWORKS; i++) {
      if (macList[i] != "") macListStr += macList[i] + "\n";
    }
    request->send(200, "text/plain", macListStr);
    Serial.println("[WEB] macList: " + macListStr);
  });

  server.on("/mymac", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", myAddresssDoted);
    Serial.println("[WEB] myMAC: " + myAddresssDoted);
  });

  server.on("/battery", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{\"level\":" + String(batteryLevel) +
                  ",\"charging\":" + String(isCharging ? "true" : "false") + "}";
    request->send(200, "application/json", json);
    Serial.printf("[WEB] battery: %d%% charging=%s\n", batteryLevel, isCharging ? "true" : "false");
  });

  // Retorna "hardcoded" o "web" per al HTML
  server.on("/configMode", HTTP_GET, [](AsyncWebServerRequest *request) {
    #ifdef HARDCODED_CONFIG
      request->send(200, "text/plain", "hardcoded");
    #else
      request->send(200, "text/plain", "web");
    #endif
  });

  // Retorna versió firmware, variant i MAC pròpia
  server.on("/info", HTTP_GET, [](AsyncWebServerRequest *request) {
    #if defined(BLAUCLICK_V1)
      const char* variant = "V1";
    #elif defined(BLAUCLICK_V2)
      const char* variant = "V2";
    #elif defined(PICO_CLICK)
      const char* variant = "PICO_CLICK";
    #endif
    String json = "{\"version\":\"" + String(FIRMWARE_VERSION) + "\","
                  "\"variant\":\"" + String(variant) + "\","
                  "\"mac\":\"" + myAddresssDoted + "\"}";
    request->send(200, "application/json", json);
  });

  server.on("/1click_cmd", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{\"cmd\":" + String(g_cmd1) +
                  ",\"p1\":"  + String(g_p1_1) +
                  ",\"p2\":"  + String(g_p2_1) +
                  ",\"p3\":"  + String(g_p3_1) + "}";
    request->send(200, "application/json", json);
    Serial.printf("[WEB] 1click_cmd: cmd=%d p1=%d p2=%d p3=%d\n", g_cmd1, g_p1_1, g_p2_1, g_p3_1);
  });

  server.on("/save_1click_cmd", HTTP_POST, [](AsyncWebServerRequest *request) {
    #ifndef HARDCODED_CONFIG
      uint8_t cmd = request->hasParam("cmd", true) ? (uint8_t)request->getParam("cmd", true)->value().toInt() : 0x01u;
      uint8_t p1  = request->hasParam("p1",  true) ? (uint8_t)request->getParam("p1",  true)->value().toInt() : 0;
      uint8_t p2  = request->hasParam("p2",  true) ? (uint8_t)request->getParam("p2",  true)->value().toInt() : 0;
      uint8_t p3  = request->hasParam("p3",  true) ? (uint8_t)request->getParam("p3",  true)->value().toInt() : 0;
      saveCmd1Click(cmd, p1, p2, p3);
    #endif
    request->send(200, "text/plain", "ok");
  });

  server.on("/disconnect-ap", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Disconnecting WiFi AP...");
    delay(1000);
    if (PIN_EN_BOTO != PIN_UNUSED) {
      digitalWrite(PIN_EN_BOTO, LOW);
    } else {
      esp_deep_sleep_start();
    }
  });

  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
    #ifndef HARDCODED_CONFIG
      int params = request->params();
      for (int i = 0; i < params; i++) {
        const AsyncWebParameter* p = request->getParam(i);
        if (p->isPost() && p->name() == PARAM_INPUT_1) {
          strMac = p->value().c_str();
          Serial.printf("[WEB] Nova MAC: %s\n", strMac.c_str());
          strMac.replace(":", "");
          if (strMac.length() > 0) {
            for (int j = 0; j < 6; j++) {
              receiverMac[j] = strtol(strMac.substring(j * 2, j * 2 + 2).c_str(), NULL, 16);
            }
            strMac = macToString(receiverMac);
            saveMac();
          }
        }
        if (p->isPost() && p->name() == "ssid") {
          receiverSSID = p->value();
          saveSSID();
          prefs.begin("blau", false);
          prefs.remove("ch");
          prefs.end();
        }
      }
    #endif
    request->send(200, "text/plain", "Configurat! Ja pots provar");
    delay(1000);
    if (PIN_EN_BOTO != PIN_UNUSED) {
      digitalWrite(PIN_EN_BOTO, LOW);
    } else {
      esp_deep_sleep_start();
    }
  });

  // Pàgines de configuració
  // server.on("/config",   HTTP_GET, [](AsyncWebServerRequest *request) {
  //   request->send(LittleFS, "/config.html",   "text/html");
  // });
  // server.on("/wifi",     HTTP_GET, [](AsyncWebServerRequest *request) {
  //   request->send(LittleFS, "/wifi.html",     "text/html");
  // });
  // server.on("/mqtt",     HTTP_GET, [](AsyncWebServerRequest *request) {
  //   request->send(LittleFS, "/mqtt.html",     "text/html");
  // });
  // server.on("/hardware", HTTP_GET, [](AsyncWebServerRequest *request) {
  //   request->send(LittleFS, "/hardware.html", "text/html");
  // });

  // Accions del dispositiu
  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Reiniciant...");
    delay(500);
    ESP.restart();
  });

  server.on("/clearconfig", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Config esborrada. Reiniciant...");
    #ifndef HARDCODED_CONFIG
      clearConfig();
    #endif
    delay(500);
    ESP.restart();
  });

  server.onNotFound(serveixWifiManager);
  server.begin();
  Serial.println("[WEB] server started");
}
