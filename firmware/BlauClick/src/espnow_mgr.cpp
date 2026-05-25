#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include "config.h"
#include "globals.h"
#include "blauprotocol.h"
#include "blauprotocol_link.h"
#include "espnow_mgr.h"

// ── Seqüències ESP-NOW (estat intern del mòdul) ──────────────────
static uint8_t       blau_seq          = 0;
volatile bool        blau_ack_received = false;
volatile uint8_t     blau_ack_seq      = 0;
volatile uint8_t     blau_ack_result   = 0;
static volatile bool _mac_delivery_ok  = false;


// ════════════════════════════════════════════════════════════════
//  ESP-NOW
// ════════════════════════════════════════════════════════════════

void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  _mac_delivery_ok = (status == ESP_NOW_SEND_SUCCESS);
  Serial.println(_mac_delivery_ok ? "[ESPNOW] ACK TX: OK" : "[ESPNOW] ACK TX: FAIL");
}

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  blau_on_data_recv(recv_info->src_addr, data, len, &blau_ack_received, &blau_ack_seq, &blau_ack_result);
}

uint8_t findBlauTriggerChannel() {
  Serial.println("[ESPNOW] Escanejant canal...");
  if (receiverSSID.length() > 0)
    Serial.printf("[ESPNOW] Cercant SSID configurat: '%s'\n", receiverSSID.c_str());
  else
    Serial.println("[ESPNOW] Cercant prefix 'BlauTrigger' (cap SSID configurat)");

  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks(false, false, false, 500);
  Serial.printf("[ESPNOW] Xarxes trobades: %d\n", n);

  uint8_t ch = 0;
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    uint8_t netCh = (uint8_t)WiFi.channel(i);
    Serial.printf("[ESPNOW]  [%d] '%s' ch=%d MAC=%s\n", i, ssid.c_str(), netCh, WiFi.BSSIDstr(i).c_str());

    bool match = (receiverSSID.length() > 0)
      ? (ssid == receiverSSID)
      : ssid.startsWith("BlauTrigger");

    if (match && ch == 0) {
      ch = netCh;
      Serial.printf("[ESPNOW] >>> Coincidencia: '%s' al canal %d\n", ssid.c_str(), ch);
    }
  }
  WiFi.scanDelete();

  if (ch == 0) {
    ch = 1;
    Serial.printf("[ESPNOW] WARN: SSID no trobat, canal per defecte: %d\n", ch);
  }
  return ch;
}

void config_ESPNOW(uint8_t channel) {
  WiFi.mode(WIFI_STA);
  Serial.printf("[ESPNOW] Configurant al canal %d\n", channel);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESPNOW] Error initializing");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("[ESPNOW] Failed to add peer");
  }
}

void send_ping() {
  BlauPacket_t pkt;
  blau_seq = millis() & 0xFF;
  blau_build_ping_packet(&pkt, blau_seq);
  blau_ack_received = false;
  esp_err_t r = esp_now_send(receiverMac, (uint8_t*)&pkt, sizeof(pkt));
  Serial.printf("[ESPNOW] PING enviat seq=%d err=0x%X\n", pkt.seq, r);
}

bool send_ESPNOW() {
  BlauPacket_t pkt;
  blau_seq = millis() & 0xFF;
  // blau_build_event_packet(&pkt, blau_seq, EVT_CLICK_1);
  blau_build_cmd_packet(&pkt, blau_seq, CMD_TOGGLE, 0,0,0);
  // blau_build_cmd_packet(&pkt, blau_seq, CMD_SET_RGB, 255,0,0);

  bool ok = blau_send_with_ack(&pkt, receiverMac, &blau_ack_received, &blau_ack_seq, &blau_ack_result);
  blau_seq = millis() & 0xFF;

  strip.setPixelColor(0, ok ? COLOR_ESPNOW_OK : COLOR_ESPNOW_FAIL);
  strip.show();

  if (!ok) Serial.println("[ESPNOW] WARN: sense ACK després de tots els intents");
  return ok;
}
