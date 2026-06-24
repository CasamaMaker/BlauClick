#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <LittleFS.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <Update.h>
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
    deleteMac();
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

  server.on("/configMode", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "web");
  });

  // Retorna versió firmware i MAC pròpia
  server.on("/info", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{\"version\":\"" + String(FIRMWARE_VERSION) + "\","
                  "\"mac\":\"" + myAddresssDoted + "\"}";
    request->send(200, "application/json", json);
  });

  server.on("/chipinfo", HTTP_GET, [](AsyncWebServerRequest *r) {
    auto rstStr = [](esp_reset_reason_t rs) -> const char* {
      switch (rs) {
        case ESP_RST_POWERON:   return "power-on";
        case ESP_RST_SW:        return "reset SW";
        case ESP_RST_PANIC:     return "excepcio/panic";
        case ESP_RST_INT_WDT:   return "WDT interrupcio";
        case ESP_RST_TASK_WDT:  return "WDT tasca";
        case ESP_RST_WDT:       return "WDT (altre)";
        case ESP_RST_DEEPSLEEP: return "deep sleep";
        case ESP_RST_BROWNOUT:  return "brownout";
        default:                return "desconegut";
      }
    };
    prefs.begin("blau", true);
    String fwFile = prefs.getString("fw_file", "");
    prefs.end();
    uint32_t up = millis() / 1000;
    char uptime[32];
    snprintf(uptime, sizeof(uptime), "%lud %02lu:%02lu:%02lu",
      (unsigned long)(up/86400), (unsigned long)((up%86400)/3600),
      (unsigned long)((up%3600)/60), (unsigned long)(up%60));
    String mac      = WiFi.macAddress();
    String ip       = WiFi.softAPIP().toString();
    String gw       = WiFi.gatewayIP().toString();
    String mask     = WiFi.subnetMask().toString();
    String dns1     = WiFi.dnsIP(0).toString();
    String dns2     = WiFi.dnsIP(1).toString();
    String ssid     = WiFi.SSID();
    int    rssi     = WiFi.RSSI();
    int    ch       = WiFi.channel();
    String bssid    = WiFi.BSSIDstr();
    String hostname = String(WiFi.getHostname());
    String json = "{";
    json += "\"fw_ver\":\""         + String(FIRMWARE_VERSION)                       + "\",";
    json += "\"fw_file\":\""        + fwFile                                          + "\",";
    json += "\"build_date\":\""     + String(__DATE__ " " __TIME__)                   + "\",";
    json += "\"uptime\":\""         + String(uptime)                                  + "\",";
    json += "\"restart_reason\":\"" + String(rstStr(esp_reset_reason()))              + "\",";
    json += "\"cpu_temp\":"         + String(temperatureRead(), 1)                    + ",";
    json += "\"chip\":\""           + String(ESP.getChipModel())                      + "\",";
    json += "\"chip_rev\":"         + String(ESP.getChipRevision())                   + ",";
    json += "\"cores\":"            + String(ESP.getChipCores())                      + ",";
    json += "\"cpu_mhz\":"          + String(ESP.getCpuFreqMHz())                     + ",";
    json += "\"idf_ver\":\""        + String(ESP.getSdkVersion())                     + "\",";
    json += "\"heap_free\":"        + String(ESP.getFreeHeap())                       + ",";
    json += "\"heap_total\":"       + String(ESP.getHeapSize())                       + ",";
    json += "\"psram_size\":"       + String(ESP.getPsramSize())                      + ",";
    json += "\"psram_free\":"       + String(ESP.getFreePsram())                      + ",";
    json += "\"flash_size\":"       + String(ESP.getFlashChipSize())                  + ",";
    json += "\"flash_mhz\":"        + String(ESP.getFlashChipSpeed()/1000000)         + ",";
    json += "\"sketch_size\":"      + String(ESP.getSketchSize())                     + ",";
    json += "\"sketch_free\":"      + String(ESP.getFreeSketchSpace())                + ",";
    json += "\"fs_used\":"          + String(LittleFS.usedBytes())                    + ",";
    json += "\"fs_total\":"         + String(LittleFS.totalBytes())                   + ",";
    json += "\"wifi_hostname\":\""  + hostname                                        + "\",";
    json += "\"wifi_ip\":\""        + ip                                              + "\",";
    json += "\"wifi_gw\":\""        + gw                                              + "\",";
    json += "\"wifi_mask\":\""      + mask                                            + "\",";
    json += "\"wifi_dns1\":\""      + dns1                                            + "\",";
    json += "\"wifi_dns2\":\""      + dns2                                            + "\",";
    json += "\"wifi_ssid\":\""      + ssid                                            + "\",";
    json += "\"wifi_rssi_dbm\":"    + String(rssi)                                    + ",";
    json += "\"wifi_ch\":"          + String(ch)                                      + ",";
    json += "\"wifi_bssid\":\""     + bssid                                           + "\",";
    json += "\"mac\":\""            + mac                                             + "\"";
    json += "}";
    r->send(200, "application/json", json);
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
    uint8_t cmd = request->hasParam("cmd", true) ? (uint8_t)request->getParam("cmd", true)->value().toInt() : 0x01u;
    uint8_t p1  = request->hasParam("p1",  true) ? (uint8_t)request->getParam("p1",  true)->value().toInt() : 0;
    uint8_t p2  = request->hasParam("p2",  true) ? (uint8_t)request->getParam("p2",  true)->value().toInt() : 0;
    uint8_t p3  = request->hasParam("p3",  true) ? (uint8_t)request->getParam("p3",  true)->value().toInt() : 0;
    saveCmd1Click(cmd, p1, p2, p3);
    request->send(200, "text/plain", "ok");
  });

  // ── Seguretat BlauProtocol v2 ─────────────────────────────────

  server.on("/securityStatus", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{\"configured\":" + String(securityConfigured() ? "true" : "false") + "}";
    request->send(200, "application/json", json);
  });

  server.on("/security", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("protopass", true)) { request->send(400, "text/plain", "missing protopass"); return; }
    String pass = request->getParam("protopass", true)->value();
    pass.trim();
    if (pass.length() < 8 || pass.length() > 63) {
      request->send(400, "text/plain", "password length 8-63");
      return;
    }
    bool ok = saveSecurityPassword(pass.c_str());   // PBKDF2 ~100 ms
    request->send(ok ? 200 : 500, "text/plain", ok ? "OK" : "ERROR");
  });

  server.on("/clearsecurity", HTTP_POST, [](AsyncWebServerRequest *request) {
    clearSecurity();
    request->send(200, "text/plain", "OK");
  });

  // ── Nom del dispositiu ────────────────────────────────────────

  server.on("/devicename", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", g_device_name);
  });

  server.on("/devicename", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("device_name", true)) {
      String name = request->getParam("device_name", true)->value();
      name.trim();
      if (name.length() > 0 && name.length() <= 32) {
        g_device_name = name;
        prefs.begin("blau", false);
        prefs.putString("devname", g_device_name);
        prefs.end();
        Serial.printf("[CFG] device_name: %s\n", g_device_name.c_str());
      }
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/cleardevicename", HTTP_POST, [](AsyncWebServerRequest *request) {
    g_device_name = WIFI_SSID;
    prefs.begin("blau", false);
    prefs.remove("devname");
    prefs.end();
    Serial.printf("[CFG] Nom del dispositiu esborrat (default: %s)\n", g_device_name.c_str());
    request->send(200, "text/plain", g_device_name.c_str());
  });

  server.on("/disconnect-ap", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Disconnecting WiFi AP...");
    delay(1000);
    if (g_pinEnBtn != PIN_UNUSED) {
      digitalWrite(g_pinEnBtn, LOW);
    } else {
      esp_deep_sleep_start();
    }
  });

  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
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
    request->send(200, "text/plain", "Configurat! Ja pots provar");
    delay(1000);
    if (g_pinEnBtn != PIN_UNUSED) {
      digitalWrite(g_pinEnBtn, LOW);
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
    clearConfig();
    delay(500);
    ESP.restart();
  });

  // ── Config hardware dinàmica ──────────────────────────────────
  server.on("/hw_gpiomap", HTTP_GET, [](AsyncWebServerRequest *request) {
    uint8_t funcMap[47] = {};
    if (g_pinEnVbat >= 0 && g_pinEnVbat <= 46) funcMap[g_pinEnVbat] = FUNC_EN_VBAT;
    if (g_pinVbat   >= 0 && g_pinVbat   <= 46) funcMap[g_pinVbat]   = FUNC_VBAT;
    if (g_pinBtn    >= 0 && g_pinBtn    <= 46) funcMap[g_pinBtn]    = FUNC_BTN;
    if (g_pinBtnInv >= 0 && g_pinBtnInv <= 46) funcMap[g_pinBtnInv] = FUNC_BTN_INV;
    if (g_pinEnBtn  >= 0 && g_pinEnBtn  <= 46) funcMap[g_pinEnBtn]  = FUNC_EN_BTN;
    if (g_pinLedDig >= 0 && g_pinLedDig <= 46) funcMap[g_pinLedDig] = FUNC_LED_DIG;
    if (g_pinLed    >= 0 && g_pinLed    <= 46) funcMap[g_pinLed]    = FUNC_LED;
    String json = "{";
    for (int i = 0; i <= 46; i++) {
      json += "\"f" + String(i) + "\":" + String(funcMap[i]) + ",";
    }
    json += "\"tmpl\":" + String(g_hwTemplate) + ",\"mcu\":\"" + String(g_hwMcu) + "\"}";
    request->send(200, "application/json", json);
  });

  server.on("/hw_gpiomap", HTTP_POST, [](AsyncWebServerRequest *request) {
    uint8_t funcMap[47] = {};
    for (int i = 0; i <= 46; i++) {
      String key = "f" + String(i);
      if (request->hasParam(key, true))
        funcMap[i] = (uint8_t)request->getParam(key, true)->value().toInt();
    }
    int8_t tmpl = -1;
    if (request->hasParam("tmpl", true))
      tmpl = (int8_t)request->getParam("tmpl", true)->value().toInt();
    String mcu = "";
    if (request->hasParam("mcu", true))
      mcu = request->getParam("mcu", true)->value();
    saveHwGpioConfig(funcMap, tmpl, mcu.c_str());
    request->send(200, "text/plain", "OK");
    delay(200);
    ESP.restart();
  });

  server.on("/hw_templates", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "[";
    for (int t = 0; t < HW_TEMPLATE_COUNT; t++) {
      if (t > 0) json += ",";
      json += "{\"name\":\"" + String(HW_TEMPLATES[t].name) + "\",\"pins\":[";
      for (int p = 0; p < HW_TEMPLATES[t].count; p++) {
        if (p > 0) json += ",";
        json += "{\"gpio\":" + String(HW_TEMPLATES[t].pins[p].gpio) +
                ",\"func\":" + String(HW_TEMPLATES[t].pins[p].func) + "}";
      }
      json += "]}";
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  server.on("/hw_funclist", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "[";
    for (int f = 0; f < FUNC_COUNT; f++) {
      if (f > 0) json += ",";
      json += "{\"id\":" + String(f) +
              ",\"label\":\"" + String(FUNC_LIST[f].label) +
              "\",\"isInput\":" + String(FUNC_LIST[f].isInput ? "true" : "false") + "}";
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  server.on("/hw_gpiocaps", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "[";
    for (int m = 0; m < MCU_PROFILE_COUNT; m++) {
      if (m > 0) json += ",";
      json += "{\"id\":\"" + String(MCU_PROFILES[m].id) +
              "\",\"name\":\"" + String(MCU_PROFILES[m].name) + "\",\"caps\":[";
      for (int g = 0; g < MCU_PROFILES[m].count; g++) {
        if (g > 0) json += ",";
        const GpioCaps& c = MCU_PROFILES[m].caps[g];
        json += "{\"valid\":"    + String(c.valid     ? "true" : "false") +
                ",\"hasPwm\":"  + String(c.hasPwm    ? "true" : "false") +
                ",\"hasAdc\":"  + String(c.hasAdc    ? "true" : "false") +
                ",\"inputOnly\":" + String(c.inputOnly ? "true" : "false") + "}";
      }
      json += "]}";
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  server.on("/hw_clear", HTTP_POST, [](AsyncWebServerRequest *request) {
    clearHwGpioConfig();
    request->send(200, "text/plain", "OK");
    delay(200);
    ESP.restart();
  });

  // ── OTA update ───────────────────────────────────────────────
  static String s_pending_fw_file;

  server.on("/ota-upload", HTTP_POST,
    [](AsyncWebServerRequest *r) {
      bool ok = !Update.hasError();
      if (ok && s_pending_fw_file.length() > 0) {
        prefs.begin("blau", false);
        prefs.putString("fw_file", s_pending_fw_file);
        prefs.end();
      }
      r->send(200, "application/json",
        ok ? "{\"ok\":true}"
           : String("{\"ok\":false,\"err\":\"") + Update.errorString() + "\"}");
      if (ok) { delay(200); ESP.restart(); }
    },
    [](AsyncWebServerRequest*, const String& filename, size_t index,
       uint8_t *data, size_t len, bool) {
      static uint32_t _app_sz, _lfs_sz, _written, _phase, _hdr_bytes;
      static uint8_t  _hdr[12];

      if (index == 0) {
        if (filename.length() > 0) s_pending_fw_file = filename;
        _app_sz = _lfs_sz = _written = _phase = _hdr_bytes = 0;
        Update.abort();
      }

      size_t off = 0;
      if (_hdr_bytes < 12) {
        size_t take = min((size_t)(12 - _hdr_bytes), len);
        memcpy(_hdr + _hdr_bytes, data, take);
        _hdr_bytes += take;
        off += take;
        if (_hdr_bytes < 12) return;
        if (memcmp(_hdr, "BLAU", 4) != 0) { Update.abort(); return; }
        memcpy(&_app_sz, _hdr + 4, 4);
        memcpy(&_lfs_sz, _hdr + 8, 4);
        if (!Update.begin(_app_sz, U_FLASH)) { Update.abort(); return; }
        _phase = 0; _written = 0;
      }

      uint8_t *ptr = data + off;
      size_t   rem = len  - off;
      while (rem > 0) {
        if (_phase == 0) {
          size_t can = min((size_t)(_app_sz - _written), rem);
          if (can > 0) { Update.write(ptr, can); _written += can; ptr += can; rem -= can; }
          if (_written >= _app_sz) {
            Update.end(true);
            if (_lfs_sz > 0 && Update.begin(_lfs_sz, U_SPIFFS)) {
              _phase = 1; _written = 0;
            } else { _phase = 2; }
          }
        } else if (_phase == 1) {
          size_t can = min((size_t)(_lfs_sz - _written), rem);
          if (can > 0) { Update.write(ptr, can); _written += can; ptr += can; rem -= can; }
          if (_written >= _lfs_sz) { Update.end(true); _phase = 2; }
        } else { break; }
      }
    }
  );

  server.onNotFound(serveixWifiManager);
  server.begin();
  Serial.println("[WEB] server started");
}
