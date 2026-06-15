/**
 * @file    blauprotocol_trg2.h
 * @brief   BlauProtocol v2 — Recepció segura al costat receptor (BlauLux)
 *
 * Implementa les mesures 3, 5, 7 i 8 de la proposta BlauProtocol v2
 * sobre el nucli criptogràfic de blauprotocol_crypto.h:
 *   - Mesura 3: nonce monotònic anti-replay per MAC
 *   - Mesura 5: rate limiting + backoff exponencial per MAC
 *   - Mesura 7: persistència diferida del nonce a NVS (màx. 1 cop/hora)
 *               + lògica testimoni + finestra post-reinici
 *   - Mesura 8: MAC whitelist amb auto-aprenentatge (TOFU)
 *
 * Després de validar i desxifrar, el paquet reconstruït v1 es processa
 * amb el pipeline existent (blau_trg_handle_packet → handleAction).
 *
 * Regles de concurrència:
 *   - blau2_trg_on_data_recv corre al context del callback ESP-NOW
 *     (task WiFi): NOMÉS RAM, zero accessos a NVS.
 *   - Les escriptures NVS (nonce, whitelist) es difereixen al loop()
 *     via blau2_trg_process_pending().
 *
 * Ús típic (espnow.cpp del receptor):
 *
 *   // setup(), després de carregar la clau AES de NVS:
 *   blau2_trg_tx_nonce_init();
 *   blau2_trg_load_state();
 *
 *   // Callback ESP-NOW:
 *   blau2_trg_on_data_recv(mac, data, len, &_ack_pending, _ack_mac,
 *                          &_ack_pkt, handleAction, state, bri, ctrl);
 *
 *   // loop():
 *   blau2_trg_process_pending(&_ack_pending, _ack_mac, &_ack_pkt, &_ack_v2);
 */

#ifndef BLAUPROTOCOL_TRG2_H
#define BLAUPROTOCOL_TRG2_H

#include "blauprotocol_trg.h"
#include "blauprotocol_crypto.h"
#include <Preferences.h>

/* =========================================================
 * Constants (proposta §4.6)
 * ========================================================= */

#define BLAU2_NONCE_WINDOW        100u        /* finestra post-reinici (mesura 7) */
#define BLAU2_NVS_WRITE_INTERVAL  3600000UL   /* 1 hora en ms (mesura 7) */
#define BLAU2_RATE_MAX_PKT        20u         /* paquets/segon màxim per MAC (mesura 5) */
#define BLAU2_BACKOFF_TIER_1      10000UL     /* 10 s  (primer abús) */
#define BLAU2_BACKOFF_TIER_2      60000UL     /* 60 s  (segon abús) */
#define BLAU2_BACKOFF_TIER_3      600000UL    /* 10 min (abús persistent) */
#define BLAU2_MAX_WHITELIST       10          /* wl_0 .. wl_9 (mesura 8) */
#define BLAU2_NVS_NAMESPACE       "blau_rx"

/* =========================================================
 * Estat per MAC emissora (tot en RAM)
 * ========================================================= */

typedef struct {
    uint32_t window_start_ms;
    uint8_t  pkt_count;
    uint8_t  backoff_tier;          /* 0 = cap, 1/2/3 = tiers */
    uint32_t block_until_ms;
} Blau2RateState_t;

typedef struct {
    uint8_t  mac[6];
    bool     used;
    uint32_t last_seen_ms;          /* per reciclar el slot més antic */
    uint32_t max_nonce;             /* màxim nonce vist en sessió (mesura 3) */
    uint32_t flash_nonce;           /* últim valor escrit a NVS (mesura 7) */
    uint32_t last_nvs_write_ms;     /* timestamp última escriptura NVS */
    bool     boot_phase;            /* true fins que el 2n paquet post-boot s'accepta */
    bool     has_witness;           /* true després del 1r paquet post-boot vàlid */
    uint32_t witness_nonce;         /* nonce del 1r paquet post-boot (no executat) */
    bool     nvs_dirty;             /* max_nonce pendent de persistir */
    bool     nvs_write_now;         /* escriptura immediata (sortida de boot_phase) */
    Blau2RateState_t rate;          /* estat rate limiting (mesura 5) */
} Blau2SenderState_t;

static Blau2SenderState_t _blau2_senders[BLAU_MAX_SOURCES];

/* Whitelist en RAM (mirall de NVS wl_0..wl_9) */
static uint8_t       _blau2_wl[BLAU2_MAX_WHITELIST][6];
static uint8_t       _blau2_wl_count = 0;
static volatile bool _blau2_wl_pending_persist = false;  /* alta TOFU pendent d'escriure */
static uint8_t       _blau2_wl_pending_idx = 0;

/* Nonce d'emissió per a les respostes xifrades (només RAM, random per boot) */
static uint32_t _blau2_tx_nonce = 0;

static bool _blau2_no_key_logged = false;

/* =========================================================
 * blau2_trg_tx_nonce_init
 *
 * Inicialitza el nonce d'emissió de respostes amb un valor
 * aleatori (mesura 4). Cridar un cop al setup().
 * ========================================================= */
static inline void blau2_trg_tx_nonce_init(void)
{
    _blau2_tx_nonce = blau_random_initial_nonce();
}

/* =========================================================
 * blau2_trg_load_state
 *
 * Precarrega de NVS ("blau_rx") la whitelist i els nonces
 * persistits per MAC, i preinicialitza els SenderState amb
 * boot_phase=true (mesura 7). Cridar un cop al setup(), després
 * de carregar la clau AES — així el callback no toca mai NVS.
 * ========================================================= */
static inline void blau2_trg_load_state(void)
{
    memset(_blau2_senders, 0x00, sizeof(_blau2_senders));
    memset(_blau2_wl, 0x00, sizeof(_blau2_wl));
    _blau2_wl_count = 0;
    _blau2_wl_pending_persist = false;

    Preferences p;
    if (!p.begin(BLAU2_NVS_NAMESPACE, true)) {
        Serial.println("[BlauTrg2] NVS blau_rx buit — whitelist en mode aprenentatge");
        return;
    }

    for (int i = 0; i < BLAU2_MAX_WHITELIST; i++) {
        char wl_key[8];
        snprintf(wl_key, sizeof(wl_key), "wl_%d", i);
        if (p.getBytesLength(wl_key) != 6) continue;

        uint8_t *mac = _blau2_wl[_blau2_wl_count];
        p.getBytes(wl_key, mac, 6);

        /* Nonce persistit per aquesta MAC → preinicialitza SenderState */
        char nc_key[20];
        snprintf(nc_key, sizeof(nc_key), "nc_%02X%02X%02X%02X%02X%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        if (_blau2_wl_count < (int)BLAU_MAX_SOURCES && p.isKey(nc_key)) {
            Blau2SenderState_t *s = &_blau2_senders[_blau2_wl_count];
            memcpy(s->mac, mac, 6);
            s->used        = true;
            s->flash_nonce = p.getUInt(nc_key, 0);
            s->max_nonce   = s->flash_nonce;
            s->boot_phase  = true;      /* mesura 7: testimoni + finestra */
            Serial.printf("[BlauTrg2] Sender %02X:%02X:%02X:%02X:%02X:%02X nonce=%lu (boot_phase)\n",
                          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                          (unsigned long)s->flash_nonce);
        }
        _blau2_wl_count++;
    }
    p.end();

    Serial.printf("[BlauTrg2] Whitelist: %d MAC(s)%s\n", _blau2_wl_count,
                  _blau2_wl_count == 0 ? " — mode aprenentatge (TOFU)" : "");
}

/* =========================================================
 * Helpers interns
 * ========================================================= */

static inline bool _blau2_mac_allowed(const uint8_t *mac)
{
    for (int i = 0; i < _blau2_wl_count; i++) {
        if (memcmp(_blau2_wl[i], mac, 6) == 0) return true;
    }
    return false;
}

static inline Blau2SenderState_t* _blau2_get_sender(const uint8_t *mac, uint32_t now)
{
    int free_slot = -1, oldest = 0;
    for (int i = 0; i < (int)BLAU_MAX_SOURCES; i++) {
        if (!_blau2_senders[i].used) {
            if (free_slot < 0) free_slot = i;
            continue;
        }
        if (memcmp(_blau2_senders[i].mac, mac, 6) == 0) {
            _blau2_senders[i].last_seen_ms = now;
            return &_blau2_senders[i];
        }
        if (_blau2_senders[i].last_seen_ms < _blau2_senders[oldest].last_seen_ms) {
            oldest = i;
        }
    }
    int target = (free_slot >= 0) ? free_slot : oldest;
    Blau2SenderState_t *s = &_blau2_senders[target];
    memset(s, 0x00, sizeof(*s));
    memcpy(s->mac, mac, 6);
    s->used         = true;
    s->last_seen_ms = now;
    /* MAC sense històric NVS: boot_phase=false — el 1r paquet s'accepta
     * directament (no hi ha passat del qual fer replay) */
    return s;
}

/* Mesura 5 — rate limiting + backoff exponencial (codi proposta §3) */
static inline bool _blau2_check_rate(Blau2RateState_t *r, uint32_t now_ms)
{
    if (now_ms < r->block_until_ms) return false;   /* bloquejat */

    if (now_ms - r->window_start_ms >= 1000u) {
        r->window_start_ms = now_ms;
        r->pkt_count = 0;
    }
    r->pkt_count++;
    if (r->pkt_count > BLAU2_RATE_MAX_PKT) {
        uint32_t delay_ms = (r->backoff_tier == 0) ? BLAU2_BACKOFF_TIER_1 :
                            (r->backoff_tier == 1) ? BLAU2_BACKOFF_TIER_2 :
                                                     BLAU2_BACKOFF_TIER_3;
        r->block_until_ms = now_ms + delay_ms;
        if (r->backoff_tier < 3) r->backoff_tier++;
        Serial.printf("[BlauTrg2] Rate limit: bloquejat %lu ms (tier %d)\n",
                      (unsigned long)delay_ms, r->backoff_tier);
        return false;
    }
    return true;
}

/* =========================================================
 * blau2_trg_on_data_recv
 *
 * Processador complet de paquets v2 (proposta §4.6). Ordre:
 *   1. mida + versió → 2. clau carregada → 3. whitelist (M8)
 *   → 4. estat per MAC → 5. rate limit (M5) → 6. GCM (M2)
 *   → 7. TOFU → 8. anti-replay (M3+M7) → 9. pipeline v1.
 *
 * Cridar des del callback ESP-NOW NOMÉS per a paquets de 21
 * bytes amb data[0] == 0x02 (el dispatch v1/v2 es fa fora).
 * Zero accessos a NVS aquí (tot diferit al loop).
 * ========================================================= */
static inline void blau2_trg_on_data_recv(const uint8_t    *mac,
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
    if (len != (int)BLAU_V2_PACKET_SIZE) return;
    if (data[0] != BLAU_PROTO_VERSION_V2) return;

    if (!blau_crypto_has_key()) {
        if (!_blau2_no_key_logged) {
            Serial.println("[BlauTrg2] Paquet v2 ignorat: cap clau configurada");
            _blau2_no_key_logged = true;
        }
        return;
    }

    /* === MESURA 8: MAC whitelist (abans de cap operació crypto) === */
    bool learning = (_blau2_wl_count == 0);
    if (!learning && !_blau2_mac_allowed(mac)) return;

    uint32_t now = (uint32_t)millis();
    Blau2SenderState_t *s = _blau2_get_sender(mac, now);

    /* === MESURA 5: rate limiting + backoff === */
    if (!_blau2_check_rate(&s->rate, now)) return;

    /* === MESURA 2: AES-128-GCM verify + decrypt === */
    BlauPacketV2_t pkt_v2;
    memcpy(&pkt_v2, data, BLAU_V2_PACKET_SIZE);

    BlauPacket_t v1pkt;
    if (!blau_v2_decrypt(&pkt_v2, &v1pkt)) {
        return;                                    /* forgery → descartar en silenci */
    }
    uint32_t nonce = pkt_v2.nonce;

    /* === TOFU: 1r emissor amb GCM vàlid s'autoregistra === */
    if (learning) {
        memcpy(_blau2_wl[0], mac, 6);
        _blau2_wl_count           = 1;
        _blau2_wl_pending_idx     = 0;
        _blau2_wl_pending_persist = true;          /* el loop l'escriu a NVS */
        Serial.printf("[BlauTrg2] TOFU: emissor %02X:%02X:%02X:%02X:%02X:%02X registrat\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    /* === MESURES 3 + 7: anti-replay + testimoni/finestra post-boot === */
    bool force_duplicate = false;

    if (s->boot_phase) {
        uint32_t X = s->flash_nonce;

        if (!s->has_witness) {
            /* 1r paquet vàlid post-boot: testimoni, mai executat */
            if (nonce > X && nonce < X + BLAU2_NONCE_WINDOW) {
                s->witness_nonce = nonce;
                s->has_witness   = true;
                Serial.printf("[BlauTrg2] Testimoni post-boot nonce=%lu (no executat)\n",
                              (unsigned long)nonce);
            }
            return;
        }
        /* 2n paquet: ha de ser > testimoni i dins la finestra */
        if (nonce <= s->witness_nonce) return;
        if (nonce <= X || nonce >= X + BLAU2_NONCE_WINDOW) return;
        s->boot_phase    = false;
        s->max_nonce     = nonce;
        s->nvs_dirty     = true;
        s->nvs_write_now = true;                   /* resincronitza NVS immediatament */
        Serial.printf("[BlauTrg2] Boot-phase superada, nonce=%lu\n", (unsigned long)nonce);
    } else {
        if (nonce < s->max_nonce) return;          /* replay → descartar en silenci */
        if (nonce == s->max_nonce && s->max_nonce != 0) {
            /* Reintent del paquet ja processat (ACK perdut):
             * el dedup v1 respondrà ACK_DUPLICATE sense executar */
            force_duplicate = true;
        } else {
            s->max_nonce = nonce;
            s->nvs_dirty = true;
        }
    }

    /* === Pipeline v1: dedup (src_id, seq) + routing + handleAction === */
    blau_print_packet(&v1pkt);
    blau_trg_handle_packet(&v1pkt, mac, ack_pending, ack_mac_out, ack_pkt_out,
                           action_cb, is_on, brightness, ctrl_type, force_duplicate);
}

/* =========================================================
 * blau2_trg_process_pending
 *
 * Cridar des de loop() en cada iteració. Fa dues feines:
 *   (a) Envia la resposta pendent (ACK/PONG/STATUS_RSP):
 *       xifrada v2 si la petició era v2, en clar v1 si no.
 *   (b) Escriptures NVS diferides (mesura 7): nonce per MAC
 *       (màx. 1 cop/hora o sortida de boot_phase) i alta TOFU.
 *
 * @param ack_pending  Flag de resposta pendent (netejat aquí)
 * @param ack_mac      MAC del destinatari
 * @param ack_pkt      Resposta v1 construïda pel pipeline
 * @param resp_is_v2   true si la petició era v2 → resposta xifrada
 * ========================================================= */
static inline void blau2_trg_process_pending(volatile bool       *ack_pending,
                                              const uint8_t       *ack_mac,
                                              const BlauPacket_t  *ack_pkt,
                                              volatile bool       *resp_is_v2)
{
    /* --- (a) resposta pendent --- */
    if (*ack_pending) {
        *ack_pending = false;

        if (!esp_now_is_peer_exist(ack_mac)) {
            esp_now_peer_info_t p = {};
            memcpy(p.peer_addr, ack_mac, 6);
            p.channel = 0;
            p.encrypt = false;
            p.ifidx   = WIFI_IF_AP;
            esp_now_add_peer(&p);
        }

        if (*resp_is_v2 && blau_crypto_has_key()) {
            BlauPacketV2_t resp_v2;
            _blau2_tx_nonce++;
            if (blau_v2_encrypt(ack_pkt, _blau2_tx_nonce, &resp_v2)) {
                esp_err_t r = esp_now_send(ack_mac, (const uint8_t *)&resp_v2,
                                           sizeof(BlauPacketV2_t));
                Serial.printf("[BlauTrg2] Resposta v2 esp_now_send: 0x%X\n", r);
            } else {
                Serial.println("[BlauTrg2] ERROR xifrant resposta");
            }
        } else {
            esp_err_t r = esp_now_send(ack_mac, (const uint8_t *)ack_pkt,
                                       sizeof(BlauPacket_t));
            Serial.printf("[BlauLux] ACK esp_now_send: 0x%X\n", r);
        }
        *resp_is_v2 = false;
    }

    /* --- (b) escriptures NVS diferides --- */
    uint32_t now = (uint32_t)millis();

    if (_blau2_wl_pending_persist) {
        _blau2_wl_pending_persist = false;
        Preferences p;
        if (p.begin(BLAU2_NVS_NAMESPACE, false)) {
            char wl_key[8];
            snprintf(wl_key, sizeof(wl_key), "wl_%d", _blau2_wl_pending_idx);
            p.putBytes(wl_key, _blau2_wl[_blau2_wl_pending_idx], 6);
            p.end();
            Serial.printf("[BlauTrg2] Whitelist %s persistida\n", wl_key);
        }
    }

    for (int i = 0; i < (int)BLAU_MAX_SOURCES; i++) {
        Blau2SenderState_t *s = &_blau2_senders[i];
        if (!s->used || !s->nvs_dirty) continue;
        if (s->max_nonce == s->flash_nonce) { s->nvs_dirty = false; continue; }
        if (!s->nvs_write_now &&
            (now - s->last_nvs_write_ms) < BLAU2_NVS_WRITE_INTERVAL) continue;

        Preferences p;
        if (!p.begin(BLAU2_NVS_NAMESPACE, false)) continue;
        char nc_key[20];
        snprintf(nc_key, sizeof(nc_key), "nc_%02X%02X%02X%02X%02X%02X",
                 s->mac[0], s->mac[1], s->mac[2], s->mac[3], s->mac[4], s->mac[5]);
        p.putUInt(nc_key, s->max_nonce);
        p.end();
        s->flash_nonce       = s->max_nonce;
        s->last_nvs_write_ms = now;
        s->nvs_dirty         = false;
        s->nvs_write_now     = false;
        Serial.printf("[BlauTrg2] NVS %s = %lu\n", nc_key, (unsigned long)s->max_nonce);
    }
}

#endif /* BLAUPROTOCOL_TRG2_H */
