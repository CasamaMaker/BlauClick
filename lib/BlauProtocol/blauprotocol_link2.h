/**
 * @file    blauprotocol_link2.h
 * @brief   BlauProtocol v2 — Enviament segur al costat emissor (BlauClick)
 *
 * Sobre el nucli criptogràfic de blauprotocol_crypto.h:
 *   - blau2_send_with_ack: xifra el paquet UNA sola vegada i reutilitza
 *     el mateix ciphertext/nonce a cada reintent (el receptor respon
 *     ACK_DUPLICATE si ja l'havia executat — el Click ho accepta com a èxit).
 *   - blau2_on_data_recv: desxifra les respostes (ACK/PONG/STATUS_RSP).
 *     Només es valida el tag GCM — sense anti-replay estricte: falsificar
 *     una resposta requereix la clau, i un replay d'ACK només afecta
 *     la lògica de reintents, no l'estat de la llum.
 *
 * Ús típic al BlauClick:
 *   BlauPacket_t pkt;
 *   blau_build_cmd_packet(&pkt, seq, CMD_TOGGLE, 0, 0, 0);   // builder v1
 *   uint32_t nonce = getNextNonce();                          // NVS "blau_tx"
 *   bool ok = blau2_send_with_ack(&pkt, nonce, receiverMac,
 *                                 &blau_ack_received, &blau_ack_seq,
 *                                 &blau_ack_result);
 */

#ifndef BLAUPROTOCOL_LINK2_H
#define BLAUPROTOCOL_LINK2_H

#include "blauprotocol_link.h"
#include "blauprotocol_crypto.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================
 * blau2_send_with_ack
 *
 * Equivalent v2 de blau_send_with_ack: xifra el paquet v1 amb
 * el nonce donat i l'envia amb reintents esperant l'ACK.
 *
 * IMPORTANT: es xifra un sol cop — els reintents reenvien
 * exactament el mateix paquet (mateix nonce/ciphertext).
 *
 * @param pkt           Paquet v1 ja construït (builders existents)
 * @param nonce         Nonce monotònic d'aquest enviament (getNextNonce)
 * @param dest_mac      MAC del receptor
 * @param ack_received  Flag volàtil escrit pel callback de recepció
 * @param ack_seq       Seq confirmat per l'ACK
 * @param ack_result    Codi ACK_* rebut
 * @return true si ACK acceptat (ACK_OK o ACK_DUPLICATE)
 * ========================================================= */
static inline bool blau2_send_with_ack(BlauPacket_t     *pkt,
                                        uint32_t          nonce,
                                        const uint8_t    *dest_mac,
                                        volatile bool    *ack_received,
                                        volatile uint8_t *ack_seq,
                                        volatile uint8_t *ack_result)
{
    BlauPacketV2_t pkt_v2;
    if (!blau_v2_encrypt(pkt, nonce, &pkt_v2)) {
        Serial.println("[BlauLnk2] ERROR xifrant paquet (cap clau configurada?)");
        return false;
    }

    for (int attempt = 0; attempt < (int)BLAU_MAX_RETRIES; attempt++) {
        *ack_received = false;

        if (esp_now_send(dest_mac, (const uint8_t *)&pkt_v2,
                         sizeof(BlauPacketV2_t)) != ESP_OK) {
            Serial.print("[BlauLnk2] esp_now_send error, intent=");
            Serial.println(attempt + 1);
            continue;
        }

        uint32_t t0 = millis();
        while (millis() - t0 < BLAU_ACK_TIMEOUT_MS) {
            if (*ack_received && *ack_seq == pkt->seq) {
                uint8_t st = *ack_result;
                if (st == ACK_OK || st == ACK_DUPLICATE) {
                    Serial.print("[BlauLnk2] ACK acceptat (intent ");
                    Serial.print(attempt + 1);
                    Serial.print(") status=0x");
                    Serial.println(st, HEX);
                    return true;
                }
                Serial.print("[BlauLnk2] ACK rebutjat, status=0x");
                Serial.println(st, HEX);
                break;
            }
            delay(1);
        }
        Serial.print("[BlauLnk2] Timeout/fail sense ACK, intent=");
        Serial.println(attempt + 1);
    }
    return false;
}

/* =========================================================
 * blau2_on_data_recv
 *
 * Processador de respostes v2 al costat emissor. Cridar des del
 * callback OnDataRecv NOMÉS per a paquets de 21 bytes amb
 * data[0] == 0x02 (el dispatch v1/v2 es fa fora).
 *
 * Verifica el tag GCM, desxifra i gestiona TYPE_ACK (flags),
 * TYPE_PONG i TYPE_STATUS_RSP — mateixa semàntica que
 * blau_on_data_recv v1.
 * ========================================================= */
static inline void blau2_on_data_recv(const uint8_t    *mac,
                                       const uint8_t    *data,
                                       int               len,
                                       volatile bool    *ack_received,
                                       volatile uint8_t *ack_seq,
                                       volatile uint8_t *ack_result)
{
    if (len != (int)BLAU_V2_PACKET_SIZE) return;
    if (data[0] != BLAU_PROTO_VERSION_V2) return;
    if (!blau_crypto_has_key()) return;

    if (!esp_now_is_peer_exist(mac)) {
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, mac, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
    }

    BlauPacketV2_t pkt_v2;
    memcpy(&pkt_v2, data, BLAU_V2_PACKET_SIZE);

    BlauPacket_t pkt;
    if (!blau_v2_decrypt(&pkt_v2, &pkt)) {
        Serial.println("[BlauLnk2] Resposta v2 amb tag invàlid, descartada");
        return;
    }
    blau_print_packet(&pkt);

    if (pkt.type == TYPE_ACK) {
        *ack_seq      = pkt.cmd;
        *ack_result   = pkt.p1;
        *ack_received = true;
    } else if (pkt.type == TYPE_PONG) {
        Serial.print("[BlauLnk2] PONG rebut, seq=");
        Serial.println(pkt.seq);
    } else if (pkt.type == TYPE_STATUS_RSP) {
        Serial.print("[BlauLnk2] STATUS_RSP: on=");
        Serial.print(pkt.p1);
        Serial.print(" bri=");
        Serial.print(pkt.p2);
        Serial.print(" type=");
        Serial.println(pkt.p3);
    } else {
        Serial.print("[BlauLnk2] Paquet inesperat, type=0x");
        Serial.println(pkt.type, HEX);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* BLAUPROTOCOL_LINK2_H */
