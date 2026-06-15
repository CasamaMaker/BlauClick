/**
 * @file    blauprotocol_crypto.h
 * @brief   BlauProtocol v2 — Nucli criptogràfic compartit (emissor i receptor)
 *
 * Implementa les mesures 1, 2 i 4 de la proposta BlauProtocol v2:
 *   - Mesura 1: PBKDF2-HMAC-SHA256 (password → clau AES de 16 bytes)
 *   - Mesura 2: AES-128-GCM (xifrat autenticat, tag truncat a 8 bytes)
 *   - Mesura 4: nonce inicial aleatori en rang [1 .. 2^28]
 *
 * Paquet v2 over-the-air (21 bytes):
 *   [VER=0x02 | CIPHERTEXT(8B) | NONCE(4B LE) | GCM_TAG(8B)]
 *    0          1-8              9-12           13-20
 *
 * El plaintext xifrat són exactament els bytes 1–8 d'un BlauPacket_t v1
 * (type, seq, src_id, cmd, p1, p2, p3) — sense version ni crc8. Així el
 * receptor, després de desxifrar, reconstrueix un BlauPacket_t v1 i pot
 * reutilitzar tot el pipeline existent (dedup, routing, handleAction).
 *
 * La contrasenya NO es persisteix mai: només la clau derivada (NVS).
 */

#ifndef BLAUPROTOCOL_CRYPTO_H
#define BLAUPROTOCOL_CRYPTO_H

#include "blauprotocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================
 * Constants criptogràfiques
 * ========================================================= */

#define BLAU_AES_KEY_LEN        16u             /* AES-128 */
#define BLAU_GCM_TAG_LEN        8u              /* tag GCM truncat */
#define BLAU_GCM_IV_LEN         4u              /* IV = nonce uint32 LE */
#define BLAU_PLAINTEXT_LEN      8u              /* bytes 1–8 del BlauPacket_t */
#define BLAU_PBKDF2_ITERATIONS  1000u
#define BLAU_PBKDF2_SALT        "BlauProtocol"  /* salt fix de protocol */
#define BLAU_PBKDF2_SALT_LEN    12u

/* =========================================================
 * Estructura del paquet v2 — sizeof MUST == BLAU_V2_PACKET_SIZE (21)
 * ========================================================= */

typedef struct __attribute__((packed)) {
    uint8_t  version;                       /* Byte 0: BLAU_PROTO_VERSION_V2 (0x02) */
    uint8_t  ciphertext[BLAU_PLAINTEXT_LEN];/* Bytes 1–8: AES-128-GCM(plaintext) */
    uint32_t nonce;                         /* Bytes 9–12: comptador monotònic, LE, en clar */
    uint8_t  tag[BLAU_GCM_TAG_LEN];         /* Bytes 13–20: GCM tag truncat */
} BlauPacketV2_t;

/* Comprovació en temps de compilació */
typedef char _blau_v2_size_check[(sizeof(BlauPacketV2_t) == BLAU_V2_PACKET_SIZE) ? 1 : -1];

/* =========================================================
 * Funcions — implementades a blauprotocol_crypto.cpp
 * ========================================================= */

/**
 * @brief  Deriva la clau AES a partir de la contrasenya (mesura 1).
 *         key = PBKDF2-HMAC-SHA256(password, "BlauProtocol", 1000, 16)
 *         Triga ~100 ms a l'ESP32. La contrasenya no es guarda enlloc.
 * @param  password  Contrasenya en text pla (NUL-terminated)
 * @param  key_out   Buffer de sortida de BLAU_AES_KEY_LEN bytes
 * @return true si la derivació ha tingut èxit
 */
bool blau_derive_key(const char *password, uint8_t key_out[BLAU_AES_KEY_LEN]);

/**
 * @brief  Carrega la clau AES al context GCM (key schedule un sol cop).
 *         Cridar al boot (clau de NVS) o en calent en configurar password.
 * @return true si la clau s'ha carregat correctament
 */
bool blau_crypto_set_key(const uint8_t key[BLAU_AES_KEY_LEN]);

/**
 * @brief  Indica si hi ha una clau carregada (mode v2 actiu).
 */
bool blau_crypto_has_key(void);

/**
 * @brief  Descarrega la clau (torna a mode v1 legacy).
 */
void blau_crypto_clear_key(void);

/**
 * @brief  Xifra un BlauPacket_t v1 en un paquet v2 (mesura 2).
 *         Plaintext = bytes 1–8 del paquet v1. IV = nonce (4 bytes LE).
 * @param  v1pkt  Paquet v1 construït amb els builders existents
 * @param  nonce  Nonce monotònic d'aquest enviament
 * @param  out    Paquet v2 de sortida (21 bytes, llest per esp_now_send)
 * @return true si el xifrat ha tingut èxit (false si no hi ha clau)
 */
bool blau_v2_encrypt(const BlauPacket_t *v1pkt, uint32_t nonce, BlauPacketV2_t *out);

/**
 * @brief  Verifica el tag GCM i desxifra un paquet v2 (mesura 2).
 *         Si és vàlid, reconstrueix un BlauPacket_t v1 (version=0x01,
 *         crc8 recalculat) perquè el pipeline v1 el pugui processar.
 * @param  in     Paquet v2 rebut
 * @param  v1out  Paquet v1 reconstruït
 * @return true si el tag és vàlid; false → forgery/corrupció (descartar)
 */
bool blau_v2_decrypt(const BlauPacketV2_t *in, BlauPacket_t *v1out);

/**
 * @brief  Genera el nonce inicial aleatori (mesura 4).
 * @return (esp_random() & 0x0FFFFFFF) + 1 — rang [1 .. 2^28]
 */
uint32_t blau_random_initial_nonce(void);

#ifdef __cplusplus
}
#endif

#endif /* BLAUPROTOCOL_CRYPTO_H */
