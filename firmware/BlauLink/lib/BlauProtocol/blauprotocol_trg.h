/**
 * @file    blauprotocol_trg.h
 * @brief   BlauProtocol v1 — Funcions d'ajuda per al costat receptor (BlauTrigger)
 *
 * Inclou NOMÉS la lògica de recepció i construcció de respostes.
 * NO inclou:
 *   - Accions de relé, llum, PWM, RGB, etc.
 *   - Lectura/escriptura EEPROM
 *   - Lògica de control de càrrega
 *
 * Ús típic al BlauTrigger (dins OnDataRecv callback):
 *
 *   void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
 *       BlauPacket_t pkt, ack;
 *
 *       if (!blau_parse_packet(data, len, &pkt)) return;   // CRC/versió KO
 *
 *       if (blau_is_duplicate(pkt.src_id, pkt.seq)) {
 *           blau_build_ack(&ack, pkt.seq, ACK_DUPLICATE);
 *           esp_now_send(mac, (uint8_t*)&ack, sizeof(ack));
 *           return;
 *       }
 *
 *       // → processar pkt.type / pkt.cmd ...
 *
 *       blau_build_ack(&ack, pkt.seq, ACK_OK);
 *       esp_now_send(mac, (uint8_t*)&ack, sizeof(ack));
 *   }
 */

#ifndef BLAUPROTOCOL_TRG_H
#define BLAUPROTOCOL_TRG_H

#include "blauprotocol.h"
#include <string.h>   /* memset */

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
 * Si qualsevol comprovació falla, retorna false i 'out' queda
 * en un estat indeterminat (no usar-lo).
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
    /* 1. Mida */
    if (len != (int)BLAU_PACKET_SIZE) {
        return false;
    }

    /* 2. Copia al struct per poder usar blau_check_crc */
    memcpy(out, data, BLAU_PACKET_SIZE);

    /* 3. CRC */
    if (!blau_check_crc(out)) {
        return false;
    }

    /* 4. Versió */
    if (out->version != BLAU_PROTO_VERSION) {
        return false;
    }

    return true;
}

/* =========================================================
 * Taula de deduplicació interna
 *
 * Guarda l'últim seq vist per a cada src_id, amb un timestamp.
 * Estàtica al mòdul (hidden linkage), adequada per a un únic
 * BlauTrigger per unitat de compilació.
 * ========================================================= */
typedef struct {
    uint16_t src_id;        /* 0x0000 indica entrada buida */
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
 * Comportament:
 *   - Si el parell és nou → registra i retorna false
 *   - Si el seq coincideix amb l'últim i està dins la finestra
 *     → és duplicat, retorna true (NO actualitza el timestamp)
 *   - Si el seq és diferent (nou event) → actualitza i retorna false
 *   - Si src_id no existeix a la taula → afegeix a la primera
 *     entrada lliure (src_id == 0) i retorna false
 *   - Si la taula és plena i src_id no hi és → substitueix
 *     l'entrada més antiga (LRU simple) i retorna false
 *
 * @param src_id   Camp src_id del paquet rebut
 * @param seq      Camp seq del paquet rebut
 * @return true si és un duplicat (descartar), false si és nou
 *
 * IMPORTANT: Requereix que millis() sigui accessible (Arduino).
 * ========================================================= */
static inline bool blau_is_duplicate(uint16_t src_id, uint8_t seq)
{
    if (!_blau_sources_init) {
        _blau_sources_reset();
    }

    uint32_t now        = (uint32_t)millis();
    int      free_slot  = -1;   /* primera entrada lliure trobada */
    int      oldest_idx = 0;    /* índex de l'entrada més antiga (LRU) */

    for (int i = 0; i < (int)BLAU_MAX_SOURCES; i++) {

        if (_blau_sources[i].src_id == 0x0000u) {
            /* Entrada lliure: anotar però continuar buscant */
            if (free_slot < 0) free_slot = i;
            continue;
        }

        /* Actualitzar candidat LRU */
        if (_blau_sources[i].last_time_ms <
            _blau_sources[oldest_idx].last_time_ms) {
            oldest_idx = i;
        }

        if (_blau_sources[i].src_id != src_id) {
            continue;   /* No és la font que busquem */
        }

        /* Entrada trobada per aquest src_id */
        uint32_t elapsed = now - _blau_sources[i].last_time_ms;

        if ((_blau_sources[i].last_seq == seq) &&
            (elapsed < BLAU_DEDUP_WINDOW_MS)) {
            /* Duplicat confirmat: NO actualitzem l'entrada */
            return true;
        }

        /* Nou seq o finestra expirada: actualitzar i acceptar */
        _blau_sources[i].last_seq      = seq;
        _blau_sources[i].last_time_ms  = now;
        return false;
    }

    /* src_id no estava a la taula: inserir */
    int target = (free_slot >= 0) ? free_slot : oldest_idx;
    _blau_sources[target].src_id       = src_id;
    _blau_sources[target].last_seq     = seq;
    _blau_sources[target].last_time_ms = now;
    return false;
}

/* =========================================================
 * blau_reset_dedup_table
 *
 * Reinicia la taula de deduplicació (útil al boot o en tests).
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
 * @param ack     Paquet ACK de sortida (inicialitzat i signat)
 * @param seq     Número de seqüència del paquet que es confirma
 *                (va al camp 'cmd' de l'ACK, per protocol)
 * @param status  Resultat de l'operació: ACK_OK / ACK_ERROR /
 *                ACK_DUPLICATE / ACK_UNAUTHORIZED / ...
 * ========================================================= */
static inline void blau_build_ack(BlauPacket_t *ack,
                                   uint8_t       seq,
                                   uint8_t       status)
{
    blau_init_packet(ack);
    ack->type = TYPE_ACK;
    ack->seq  = seq;        /* seq propi del Trigger (pot ser 0 en v1) */
    ack->cmd  = seq;        /* seq confirmat — camp 'cmd' per protocol */
    ack->p1   = status;     /* ACK_OK, ACK_DUPLICATE, ACK_ERROR, ... */
    ack->p2   = 0x00u;      /* codi d'error addicional (futur) */
    ack->p3   = 0x00u;
    blau_fill_crc(ack);
}

/* =========================================================
 * blau_build_pong
 *
 * Construeix un paquet TYPE_PONG en resposta a un TYPE_PING.
 *
 * @param pong  Paquet PONG de sortida
 * @param seq   Número de seqüència del PING rebut (es reflecteix)
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
 * Construeix un paquet TYPE_STATUS_RSP amb l'estat actual del
 * BlauTrigger.
 *
 * @param rsp          Paquet de sortida
 * @param seq          Número de seqüència de la petició rebuda
 * @param is_on        true si la càrrega està encesa
 * @param brightness   Brillantor actual 0–100 (%)
 * @param control_type Tipus de càrrega configurat (0=relay, 1=digled,
 *                     2=pwm, 3=cct, 4=rgb, ...)
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
    rsp->p1   = is_on ? 0x01u : 0x00u;   /* bit 0 = on/off */
    rsp->p2   = brightness;               /* 0–100 */
    rsp->p3   = control_type;             /* tipus de càrrega */
    blau_fill_crc(rsp);
}

#ifdef __cplusplus
}
#endif

#endif /* BLAUPROTOCOL_TRG_H */
