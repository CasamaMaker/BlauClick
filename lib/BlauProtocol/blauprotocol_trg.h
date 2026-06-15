/**
 * @file    blauprotocol_trg.h
 * @brief   BlauProtocol v1 — Funcions d'ajuda per al costat receptor (BlauLux)
 *
 * Inclou:
 *   - Validació i parsejat de paquets rebuts (blau_parse_packet)
 *   - Deduplicació per src_id + seq (blau_is_duplicate)
 *   - Construcció de respostes: ACK, PONG, STATUS_RSP
 *   - Impressió de paquets per depuració (blau_print_packet)
 *   - Processador complet de recepció (blau_trg_on_data_recv)
 *   - Enviament de resposta pendent des de loop() (blau_trg_process_pending)
 *
 * NO inclou:
 *   - Accions de relé, llum, PWM, RGB, etc.
 *   - Lectura/escriptura EEPROM
 *   - Lògica de control de càrrega
 *
 * Ús típic al BlauLux:
 *
 *   // Callback d'acció — implementar a main.cpp
 *   uint8_t handleAction(uint8_t type, uint8_t cmd,
 *                        uint8_t p1, uint8_t p2, uint8_t p3) {
 *       if (type == TYPE_EVENT && cmd == EVT_CLICK_1) {
 *           controlLlum("espnow");
 *           return ACK_OK;
 *       }
 *       return ACK_ERROR;
 *   }
 *
 *   // Callback ESP-NOW
 *   void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
 *       blau_trg_on_data_recv(mac, data, len,
 *                             &_ack_pending, _ack_mac, &_ack_pkt,
 *                             handleAction,
 *                             state, brightness, control_type);
 *   }
 *
 *   // Al loop()
 *   blau_trg_process_pending(&_ack_pending, _ack_mac, &_ack_pkt);
 */

#ifndef BLAUPROTOCOL_TRG_H
#define BLAUPROTOCOL_TRG_H

#include "blauprotocol.h"
#include <string.h>    /* memcpy, memset */
#include <esp_now.h>   /* blau_trg_on_data_recv, blau_trg_process_pending */
#include <Arduino.h>   /* Serial, millis */

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================
 * blau_parse_packet
 *
 * Valida i deserialitza un buffer rebut via ESP-NOW en una
 * estructura BlauPacket_t. Realitza en ordre:
 *   1. Comprovació de mida (ha de ser BLAU_PACKET_SIZE)
 *   2. Comprovació de CRC-8
 *   3. Comprovació de versió de protocol
 *
 * @param data  Buffer rebut des del callback ESP-NOW
 * @param len   Longitud del buffer en bytes
 * @param out   Paquet de sortida si la validació té èxit
 * @return true si el paquet és vàlid i pot processar-se
 * ========================================================= */
static inline bool blau_parse_packet(const uint8_t  *data,
                                      int             len,
                                      BlauPacket_t   *out)
{
    if (len != (int)BLAU_PACKET_SIZE) return false;
    memcpy(out, data, BLAU_PACKET_SIZE);
    if (!blau_check_crc(out)) return false;
    if (out->version != BLAU_PROTO_VERSION) return false;
    return true;
}

/* =========================================================
 * Taula de deduplicació interna
 * ========================================================= */
typedef struct {
    uint16_t src_id;
    uint8_t  last_seq;
    uint32_t last_time_ms;
} _BlauSourceRecord_t;

static _BlauSourceRecord_t _blau_sources[BLAU_MAX_SOURCES];
static bool                _blau_sources_init = false;

static inline void _blau_sources_reset(void)
{
    memset(_blau_sources, 0x00, sizeof(_blau_sources));
    _blau_sources_init = true;
}

/* =========================================================
 * blau_is_duplicate
 *
 * Comprova si el parell (src_id, seq) ja ha estat processat
 * dins de la finestra temporal BLAU_DEDUP_WINDOW_MS.
 *
 * @return true si és un duplicat (descartar), false si és nou
 * ========================================================= */
static inline bool blau_is_duplicate(uint16_t src_id, uint8_t seq)
{
    if (!_blau_sources_init) _blau_sources_reset();

    uint32_t now        = (uint32_t)millis();
    int      free_slot  = -1;
    int      oldest_idx = 0;

    for (int i = 0; i < (int)BLAU_MAX_SOURCES; i++) {
        if (_blau_sources[i].src_id == 0x0000u) {
            if (free_slot < 0) free_slot = i;
            continue;
        }
        if (_blau_sources[i].last_time_ms <
            _blau_sources[oldest_idx].last_time_ms) {
            oldest_idx = i;
        }
        if (_blau_sources[i].src_id != src_id) continue;

        uint32_t elapsed = now - _blau_sources[i].last_time_ms;
        if ((_blau_sources[i].last_seq == seq) &&
            (elapsed < BLAU_DEDUP_WINDOW_MS)) {
            return true;
        }
        _blau_sources[i].last_seq     = seq;
        _blau_sources[i].last_time_ms = now;
        return false;
    }

    int target = (free_slot >= 0) ? free_slot : oldest_idx;
    _blau_sources[target].src_id       = src_id;
    _blau_sources[target].last_seq     = seq;
    _blau_sources[target].last_time_ms = now;
    return false;
}

/* =========================================================
 * blau_reset_dedup_table
 * ========================================================= */
static inline void blau_reset_dedup_table(void)
{
    _blau_sources_reset();
}

/* =========================================================
 * blau_build_ack
 *
 * Construeix un paquet TYPE_ACK per respondre a un paquet rebut.
 *
 * @param seq     Número de seqüència del paquet que es confirma
 * @param status  ACK_OK / ACK_ERROR / ACK_DUPLICATE / ...
 * ========================================================= */
static inline void blau_build_ack(BlauPacket_t *ack,
                                   uint8_t       seq,
                                   uint8_t       status)
{
    blau_init_packet(ack);
    ack->type = TYPE_ACK;
    ack->seq  = seq;
    ack->cmd  = seq;       /* seq confirmat — per protocol, va al camp cmd */
    ack->p1   = status;
    ack->p2   = 0x00u;
    ack->p3   = 0x00u;
    blau_fill_crc(ack);
}

/* =========================================================
 * blau_build_pong
 *
 * Construeix un paquet TYPE_PONG en resposta a un TYPE_PING.
 * ========================================================= */
static inline void blau_build_pong(BlauPacket_t *pong, uint8_t seq)
{
    blau_init_packet(pong);
    pong->type = TYPE_PONG;
    pong->seq  = seq;
    pong->cmd  = 0x00u;
    blau_fill_crc(pong);
}

/* =========================================================
 * blau_build_status_rsp
 *
 * Construeix un paquet TYPE_STATUS_RSP amb l'estat actual.
 *
 * @param is_on        true si la càrrega està encesa
 * @param brightness   Brillantor actual 0–100 (%)
 * @param control_type 0=relay, 1=digled, 2=pwm, 3=cct, ...
 * ========================================================= */
static inline void blau_build_status_rsp(BlauPacket_t *rsp,
                                          uint8_t       seq,
                                          bool          is_on,
                                          uint8_t       brightness,
                                          uint8_t       control_type)
{
    blau_init_packet(rsp);
    rsp->type = TYPE_STATUS_RSP;
    rsp->seq  = seq;
    rsp->cmd  = 0x00u;
    rsp->p1   = is_on ? 0x01u : 0x00u;
    rsp->p2   = brightness;
    rsp->p3   = control_type;
    blau_fill_crc(rsp);
}

/* =========================================================
 * Helpers interns de noms llegibles — no formen part de l'API pública
 * ========================================================= */

static inline const char* _blau_type_label(uint8_t type)
{
    switch (type) {
        case TYPE_EVENT:      return "TYPE_EVENT      : Event de polsador";
        case TYPE_CMD:        return "TYPE_CMD        : Comanda directa";
        case TYPE_ACK:        return "TYPE_ACK        : Confirmació d'execució";
        case TYPE_PING:       return "TYPE_PING       : Comprovació de presència";
        case TYPE_PONG:       return "TYPE_PONG       : Resposta al ping";
        case TYPE_STATUS_REQ: return "TYPE_STATUS_REQ : Sol·licitud d'estat";
        case TYPE_STATUS_RSP: return "TYPE_STATUS_RSP : Resposta d'estat";
        default:              return "TYPE_?          : Desconegut";
    }
}

static inline const char* _blau_cmd_label(uint8_t type, uint8_t cmd)
{
    if (type == TYPE_EVENT) {
        switch (cmd) {
            case EVT_CLICK_1:    return "EVT_CLICK_1    : 1 clic simple";
            case EVT_CLICK_2:    return "EVT_CLICK_2    : 2 clics ràpids";
            case EVT_CLICK_3:    return "EVT_CLICK_3    : 3 clics ràpids";
            case EVT_LONG_START: return "EVT_LONG_START : Inici pulsació llarga";
            case EVT_LONG_END:   return "EVT_LONG_END   : Fi pulsació llarga";
            default:             return "EVT_?          : Desconegut";
        }
    }
    if (type == TYPE_CMD) {
        switch (cmd) {
            case CMD_TOGGLE:         return "CMD_TOGGLE         : Toggle on/off";
            case CMD_ON:             return "CMD_ON             : Encendre";
            case CMD_OFF:            return "CMD_OFF            : Apagar";
            case CMD_SET_BRIGHTNESS: return "CMD_SET_BRIGHTNESS : Brillantor (p1=%)";
            case CMD_SET_RGB:        return "CMD_SET_RGB        : Color RGB (p1=R p2=G p3=B)";
            case CMD_SET_CCT:        return "CMD_SET_CCT        : Temperatura color";
            case CMD_SET_SCENE:      return "CMD_SET_SCENE      : Escena (p1=id)";
            case CMD_DIM_UP:         return "CMD_DIM_UP         : Puja brillantor";
            case CMD_DIM_DOWN:       return "CMD_DIM_DOWN       : Baixa brillantor";
            default:                 return "CMD_?              : Desconeguda";
        }
    }
    if (type == TYPE_ACK) return "(seq confirmat)";
    return "-";
}

static inline const char* _blau_ack_label(uint8_t status)
{
    switch (status) {
        case ACK_OK:           return "ACK_OK           : Processat correctament";
        case ACK_ERROR:        return "ACK_ERROR        : Error d'execució";
        case ACK_DUPLICATE:    return "ACK_DUPLICATE    : Duplicat, ignorat";
        case ACK_UNAUTHORIZED: return "ACK_UNAUTHORIZED : No autoritzat";
        case ACK_BAD_VERSION:  return "ACK_BAD_VERSION  : Versió incompatible";
        case ACK_BAD_CRC:      return "ACK_BAD_CRC      : CRC incorrecte";
        default:               return "ACK_?            : Desconegut";
    }
}

/* =========================================================
 * blau_print_packet
 *
 * Imprimeix per Serial el contingut d'un paquet BlauProtocol
 * amb noms llegibles per als camps semàntics (type, cmd, p1 ACK).
 * Requereix que Serial estigui inicialitzat.
 * ========================================================= */
static inline void blau_print_packet(const BlauPacket_t *pkt)
{
    Serial.println("---- PACKET ----");
    Serial.printf("version: 0x%02X\n",                                              pkt->version);
    Serial.printf("type:    %s : 0x%02X\n", _blau_type_label(pkt->type),            pkt->type);
    Serial.printf("seq:     %d\n",                                                   pkt->seq);
    Serial.printf("cmd:     %s : 0x%02X\n", _blau_cmd_label(pkt->type, pkt->cmd),   pkt->cmd);
    if (pkt->type == TYPE_ACK) {
        Serial.printf("p1:      %s : 0x%02X\n", _blau_ack_label(pkt->p1),           pkt->p1);
    } else {
        Serial.printf("p1:      0x%02X\n",                                           pkt->p1);
    }
    Serial.printf("p2:      0x%02X\n", pkt->p2);
    Serial.printf("p3:      0x%02X\n", pkt->p3);
    Serial.printf("src_id:  0x%04X\n", pkt->src_id);
    Serial.printf("crc:     0x%02X\n", pkt->crc8);
    Serial.println("----------------");
}

/* =========================================================
 * blau_action_fn_t
 *
 * Prototip del callback d'acció que el BlauLux ha de
 * implementar a main.cpp. Rep el tipus i comanda del paquet
 * i retorna el codi ACK_* resultant.
 *
 * @param pkt_type  TYPE_EVENT o TYPE_CMD
 * @param cmd       EVT_* o CMD_* segons el tipus
 * @param p1..p3    Paràmetres del paquet
 * @return          ACK_OK, ACK_ERROR, etc.
 * ========================================================= */
typedef uint8_t (*blau_action_fn_t)(uint8_t pkt_type, uint8_t cmd,
                                     uint8_t p1, uint8_t p2, uint8_t p3);

/* =========================================================
 * blau_trg_handle_packet
 *
 * Nucli de processament d'un paquet ja validat (post-parse o
 * post-desxifrat v2): deduplicació, routing per tipus,
 * construcció de resposta (ACK/PONG/STATUS_RSP) i cua pendent.
 *
 * @param pkt              Paquet validat (v1 natiu o reconstruït des de v2)
 * @param mac              MAC del remitent
 * @param ack_pending      Flag de resposta pendent (escrit aquí, llegit al loop)
 * @param ack_mac_out      Buffer de 6 bytes on guardar la MAC destinatària
 * @param ack_pkt_out      Paquet de resposta a enviar
 * @param action_cb        Callback per executar l'acció (EVT/CMD → ACK status)
 * @param is_on            Estat actual de la càrrega (per STATUS_RSP)
 * @param brightness       Brillantor actual 0–100 (per STATUS_RSP)
 * @param ctrl_type        Tipus de control configurat (per STATUS_RSP)
 * @param force_duplicate  true → tractar com a duplicat sense executar
 *                         (usat per v2 quan nonce == max_nonce: reintent
 *                         d'un paquet ja executat → respondre ACK_DUPLICATE)
 * ========================================================= */
static inline void blau_trg_handle_packet(const BlauPacket_t *pkt_in,
                                           const uint8_t      *mac,
                                           volatile bool      *ack_pending,
                                           uint8_t            *ack_mac_out,
                                           BlauPacket_t       *ack_pkt_out,
                                           blau_action_fn_t    action_cb,
                                           bool                is_on,
                                           uint8_t             brightness,
                                           uint8_t             ctrl_type,
                                           bool                force_duplicate)
{
    BlauPacket_t pkt;
    memcpy(&pkt, pkt_in, sizeof(BlauPacket_t));

    bool    dup      = force_duplicate || blau_is_duplicate(pkt.src_id, pkt.seq);
    uint8_t ack_s    = ACK_OK;
    bool    need_ack = true;

    switch (pkt.type) {

        case TYPE_EVENT:
        case TYPE_CMD:
            if (dup) {
                ack_s = ACK_DUPLICATE;
                Serial.println("[BlauLux] Duplicat ignorat");
            } else {
                ack_s = action_cb(pkt.type, pkt.cmd, pkt.p1, pkt.p2, pkt.p3);
            }
            break;

        case TYPE_PING:
            blau_build_pong(ack_pkt_out, pkt.seq);
            memcpy(ack_mac_out, mac, 6);
            *ack_pending = true;
            need_ack = false;
            Serial.print("[BlauLux] PING rebut, PONG pendent seq=");
            Serial.println(pkt.seq);
            break;

        case TYPE_STATUS_REQ:
            blau_build_status_rsp(ack_pkt_out, pkt.seq, is_on, brightness, ctrl_type);
            memcpy(ack_mac_out, mac, 6);
            *ack_pending = true;
            need_ack = false;
            Serial.println("[BlauLux] STATUS_REQ rebut, STATUS_RSP pendent");
            break;

        default:
            Serial.print("[BlauLux] Tipus desconegut ignorat: 0x");
            Serial.println(pkt.type, HEX);
            need_ack = false;
            break;
    }

    if (need_ack) {
        memcpy(ack_mac_out, mac, 6);
        blau_build_ack(ack_pkt_out, pkt.seq, ack_s);
        *ack_pending = true;
        Serial.print("[BlauLux] ACK pendent (");
        Serial.print(ack_s == ACK_OK        ? "OK"  :
                     ack_s == ACK_DUPLICATE ? "DUP" : "ERR");
        Serial.print(") seq=");
        Serial.println(pkt.seq);
    }
}

/* =========================================================
 * blau_trg_on_data_recv
 *
 * Processador complet de paquets v1 rebuts al costat BlauLux.
 * Cridar des del callback OnDataRecv d'ESP-NOW.
 *
 * Valida el paquet (mida, CRC, versió) i delega el processament
 * a blau_trg_handle_packet().
 *
 * IMPORTANT: és cridada des d'un context d'interrupció (ESP-NOW
 * callback). No fa operacions bloquejants. L'enviament de la
 * resposta es fa des de loop() via blau_trg_process_pending().
 *
 * @param mac          MAC del remitent
 * @param data         Buffer de dades rebut
 * @param len          Longitud del buffer
 * @param ack_pending  Flag de resposta pendent (escrit aquí, llegit al loop)
 * @param ack_mac_out  Buffer de 6 bytes on guardar la MAC destinatària
 * @param ack_pkt_out  Paquet de resposta a enviar
 * @param action_cb    Callback per executar l'acció (EVT/CMD → ACK status)
 * @param is_on        Estat actual de la càrrega (per STATUS_RSP)
 * @param brightness   Brillantor actual 0–100 (per STATUS_RSP)
 * @param ctrl_type    Tipus de control configurat (per STATUS_RSP)
 * ========================================================= */
static inline void blau_trg_on_data_recv(const uint8_t    *mac,
                                          const uint8_t    *data,
                                          int               len,
                                          volatile bool    *ack_pending,
                                          uint8_t          *ack_mac_out,
                                          BlauPacket_t     *ack_pkt_out,
                                          blau_action_fn_t  action_cb,
                                          bool              is_on,
                                          uint8_t           brightness,
                                          uint8_t           ctrl_type)
{
    BlauPacket_t pkt;
    if (!blau_parse_packet(data, len, &pkt)) {
        Serial.println("[BlauLux] Paquet invàlid (mida, CRC o versió)");
        return;
    }
    blau_print_packet(&pkt);

    blau_trg_handle_packet(&pkt, mac, ack_pending, ack_mac_out, ack_pkt_out,
                           action_cb, is_on, brightness, ctrl_type, false);
}

/* =========================================================
 * blau_trg_process_pending
 *
 * Envia la resposta pendent (ACK/PONG/STATUS_RSP) via ESP-NOW.
 * Cridar des de loop() en cada iteració.
 *
 * Registra automàticament el peer si cal (WIFI_IF_AP per BlauLux).
 *
 * @param ack_pending  Flag de resposta pendent (netejat aquí)
 * @param ack_mac      MAC del destinatari
 * @param ack_pkt      Paquet a enviar
 * ========================================================= */
static inline void blau_trg_process_pending(volatile bool       *ack_pending,
                                              const uint8_t       *ack_mac,
                                              const BlauPacket_t  *ack_pkt)
{
    if (!*ack_pending) return;
    *ack_pending = false;

    if (!esp_now_is_peer_exist(ack_mac)) {
        esp_now_peer_info_t p = {};
        memcpy(p.peer_addr, ack_mac, 6);
        p.channel = 0;
        p.encrypt = false;
        p.ifidx   = WIFI_IF_AP;
        esp_now_add_peer(&p);
    }
    esp_err_t r = esp_now_send(ack_mac, (const uint8_t *)ack_pkt, sizeof(BlauPacket_t));
    Serial.print("[BlauLux] ACK esp_now_send: 0x");
    Serial.println(r, HEX);
}

#ifdef __cplusplus
}
#endif

#endif /* BLAUPROTOCOL_TRG_H */
