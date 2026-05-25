/**
 * @file    blauprotocol_link.h
 * @brief   BlauProtocol v1 — Funcions d'ajuda per al costat emissor (BlauClick)
 *
 * Inclou:
 *   - Construcció de paquets EVENT, CMD, PING, STATUS_REQ
 *   - Comprovació d'ACK rebut (blau_is_ack_for, blau_ack_status)
 *   - Enviament amb reintents i espera d'ACK (blau_send_with_ack)
 *   - Processador de recepció del costat Link (blau_on_data_recv)
 *
 * Nota: inclou blauprotocol_trg.h, que proporciona blau_parse_packet,
 * blau_print_packet i tota la lògica de recepció/validació compartida.
 *
 * Ús típic al BlauClick:
 *   BlauPacket_t pkt;
 *   uint8_t seq = millis() & 0xFF;
 *   blau_build_event_packet(&pkt, seq, EVT_CLICK_1);
 *   bool ok = blau_send_with_ack(&pkt, receiverMac,
 *                                 &blau_ack_received,
 *                                 &blau_ack_seq,
 *                                 &blau_ack_result);
 */

#ifndef BLAUPROTOCOL_LINK_H
#define BLAUPROTOCOL_LINK_H

#include "blauprotocol_trg.h"   /* inclou blauprotocol.h, esp_now.h, Arduino.h, string.h */

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================
 * blau_build_event_packet
 *
 * Construeix un paquet TYPE_EVENT per enviar un event de polsador.
 *
 * @param pkt    Paquet de sortida (inicialitzat i signat)
 * @param seq    Número de seqüència del missatge (0–255)
 * @param event  EVT_CLICK_1 / EVT_CLICK_2 / EVT_CLICK_3
 *               EVT_LONG_START / EVT_LONG_END
 *
 * Nota: p3 es reserva per al nivell de bateria (0–100, futur).
 * ========================================================= */
static inline void blau_build_event_packet(BlauPacket_t *pkt,
                                            uint8_t       seq,
                                            uint8_t       event)
{
    blau_init_packet(pkt);
    pkt->type = TYPE_EVENT;
    pkt->seq  = seq;
    pkt->cmd  = event;
    pkt->p1   = 0x00u;
    pkt->p2   = 0x00u;
    pkt->p3   = 0x00u;
    blau_fill_crc(pkt);
}

/* =========================================================
 * blau_build_cmd_packet
 *
 * Construeix un paquet TYPE_CMD per enviar una comanda directa.
 *
 * Exemples:
 *   Toggle:          blau_build_cmd_packet(&pkt, seq, CMD_TOGGLE, 0,0,0)
 *   Brillantor 75%:  blau_build_cmd_packet(&pkt, seq, CMD_SET_BRIGHTNESS, 75,0,0)
 *   Color vermell:   blau_build_cmd_packet(&pkt, seq, CMD_SET_RGB, 255,0,0)
 * ========================================================= */
static inline void blau_build_cmd_packet(BlauPacket_t *pkt,
                                          uint8_t       seq,
                                          uint8_t       cmd,
                                          uint8_t       p1,
                                          uint8_t       p2,
                                          uint8_t       p3)
{
    blau_init_packet(pkt);
    pkt->type = TYPE_CMD;
    pkt->seq  = seq;
    pkt->cmd  = cmd;
    pkt->p1   = p1;
    pkt->p2   = p2;
    pkt->p3   = p3;
    blau_fill_crc(pkt);
}

/* =========================================================
 * blau_build_ping_packet
 * ========================================================= */
static inline void blau_build_ping_packet(BlauPacket_t *pkt, uint8_t seq)
{
    blau_init_packet(pkt);
    pkt->type = TYPE_PING;
    pkt->seq  = seq;
    pkt->cmd  = 0x00u;
    blau_fill_crc(pkt);
}

/* =========================================================
 * blau_build_status_req_packet
 * ========================================================= */
static inline void blau_build_status_req_packet(BlauPacket_t *pkt, uint8_t seq)
{
    blau_init_packet(pkt);
    pkt->type = TYPE_STATUS_REQ;
    pkt->seq  = seq;
    pkt->cmd  = 0x00u;
    blau_fill_crc(pkt);
}

/* =========================================================
 * blau_is_ack_for
 *
 * Comprova si el paquet rebut és un ACK vàlid per al seq esperat.
 *
 * @param  received  Paquet rebut (ja verificat amb blau_check_crc)
 * @param  seq       Número de seqüència que s'espera confirmar
 * @return true si és un ACK per a 'seq'
 * ========================================================= */
static inline bool blau_is_ack_for(const BlauPacket_t *received, uint8_t seq)
{
    return (received->type == TYPE_ACK) && (received->cmd == seq);
}

/* =========================================================
 * blau_ack_status
 *
 * Extreu el codi de resultat d'un paquet ACK (camp p1).
 * ========================================================= */
static inline uint8_t blau_ack_status(const BlauPacket_t *ack)
{
    return ack->p1;
}

/* =========================================================
 * blau_send_with_ack
 *
 * Envia un paquet BlauProtocol via ESP-NOW amb reintents i
 * espera l'ACK corresponent.
 *
 * Reintenta fins a BLAU_MAX_RETRIES vegades, esperant
 * BLAU_ACK_TIMEOUT_MS per resposta. Accepta ACK_OK i
 * ACK_DUPLICATE com a èxit.
 *
 * IMPORTANT: el mateix seq ha de reutilitzar-se a cada reintent.
 *
 * @param pkt           Paquet ja construït i signat (CRC omplert)
 * @param dest_mac      MAC del dispositiu receptor (6 bytes)
 * @param ack_received  Punter al flag volàtil escrit per OnDataRecv
 * @param ack_seq       Punter al seq de l'ACK rebut
 * @param ack_result    Punter al codi ACK_* rebut
 * @return true si ACK acceptat, false si timeout esgotat
 * ========================================================= */
static inline bool blau_send_with_ack(BlauPacket_t     *pkt,
                                       const uint8_t    *dest_mac,
                                       volatile bool    *ack_received,
                                       volatile uint8_t *ack_seq,
                                       volatile uint8_t *ack_result)
{
    for (int attempt = 0; attempt < (int)BLAU_MAX_RETRIES; attempt++) {
        *ack_received = false;

        if (esp_now_send(dest_mac, (uint8_t *)pkt, sizeof(BlauPacket_t)) != ESP_OK) {
            Serial.print("[BlauClick] esp_now_send error, intent=");
            Serial.println(attempt + 1);
            continue;
        }

        uint32_t t0 = millis();
        while (millis() - t0 < BLAU_ACK_TIMEOUT_MS) {
            if (*ack_received && *ack_seq == pkt->seq) {
                uint8_t st = *ack_result;
                if (st == ACK_OK || st == ACK_DUPLICATE) {
                    Serial.print("[BlauClick] ACK acceptat (intent ");
                    Serial.print(attempt + 1);
                    Serial.print(") status=0x");
                    Serial.println(st, HEX);
                    return true;
                }
                Serial.print("[BlauClick] ACK rebutjat, status=0x");
                Serial.println(st, HEX);
                break;
            }
            delay(1);
        }
        Serial.print("[BlauClick] Timeout/fail sense ACK, intent=");
        Serial.println(attempt + 1);
    }
    return false;
}

/* =========================================================
 * blau_on_data_recv
 *
 * Processador de paquets rebuts al costat BlauClick.
 * Cridar des del callback OnDataRecv d'ESP-NOW.
 *
 * Gestiona: TYPE_ACK (actualitza flags), TYPE_PONG, TYPE_STATUS_RSP.
 * Afegeix automàticament el peer si no estava registrat.
 *
 * IMPORTANT: cridada des d'un context d'interrupció. No fa
 * operacions bloquejants.
 *
 * @param mac           MAC del remitent
 * @param data          Buffer de dades rebut
 * @param len           Longitud del buffer
 * @param ack_received  Flag volàtil d'ACK rebut
 * @param ack_seq       Seq confirmat per l'ACK
 * @param ack_result    Codi ACK_* rebut
 * ========================================================= */
static inline void blau_on_data_recv(const uint8_t    *mac,
                                      const uint8_t    *data,
                                      int               len,
                                      volatile bool    *ack_received,
                                      volatile uint8_t *ack_seq,
                                      volatile uint8_t *ack_result)
{
    if (!esp_now_is_peer_exist(mac)) {
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, mac, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
    }

    BlauPacket_t pkt;
    if (!blau_parse_packet(data, len, &pkt)) {
        Serial.print("[BlauClick] Paquet invàlid (mida, CRC o versió), len=");
        Serial.println(len);
        return;
    }
    blau_print_packet(&pkt);

    if (pkt.type == TYPE_ACK) {
        *ack_seq      = pkt.cmd;
        *ack_result   = pkt.p1;
        *ack_received = true;
    } else if (pkt.type == TYPE_PONG) {
        Serial.print("[BlauClick] PONG rebut, seq=");
        Serial.println(pkt.seq);
    } else if (pkt.type == TYPE_STATUS_RSP) {
        Serial.print("[BlauClick] STATUS_RSP: on=");
        Serial.print(pkt.p1);
        Serial.print(" bri=");
        Serial.print(pkt.p2);
        Serial.print(" type=");
        Serial.println(pkt.p3);
    } else {
        Serial.print("[BlauClick] Paquet inesperat, type=0x");
        Serial.println(pkt.type, HEX);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* BLAUPROTOCOL_LINK_H */
