/**
 * @file    blauprotocol_crypto.cpp
 * @brief   BlauProtocol v2 — Implementació del nucli criptogràfic
 *
 * Vegeu blauprotocol_crypto.h per a la documentació de l'API.
 */

#include "blauprotocol_crypto.h"

#include <string.h>

#include "mbedtls/gcm.h"
#include "mbedtls/md.h"
#include "mbedtls/pkcs5.h"
#include "mbedtls/version.h"
#include "esp_random.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* =========================================================
 * Estat intern
 *
 * El context GCM es comparteix entre el callback ESP-NOW (task
 * WiFi) i el loop() — cal mutex perquè mbedTLS no és reentrant
 * sobre el mateix context.
 * ========================================================= */

static mbedtls_gcm_context _gcm_ctx;
static bool                _key_set = false;
static SemaphoreHandle_t   _gcm_mutex = NULL;

static void _lock(void)
{
    if (_gcm_mutex != NULL) xSemaphoreTake(_gcm_mutex, portMAX_DELAY);
}

static void _unlock(void)
{
    if (_gcm_mutex != NULL) xSemaphoreGive(_gcm_mutex);
}

/* =========================================================
 * blau_derive_key — Mesura 1 (PBKDF2)
 * ========================================================= */
bool blau_derive_key(const char *password, uint8_t key_out[BLAU_AES_KEY_LEN])
{
    if (password == NULL || password[0] == '\0') return false;

    int ret;
#if MBEDTLS_VERSION_MAJOR >= 3
    ret = mbedtls_pkcs5_pbkdf2_hmac_ext(
        MBEDTLS_MD_SHA256,
        (const unsigned char *)password, strlen(password),
        (const unsigned char *)BLAU_PBKDF2_SALT, BLAU_PBKDF2_SALT_LEN,
        BLAU_PBKDF2_ITERATIONS,
        BLAU_AES_KEY_LEN, key_out);
#else
    mbedtls_md_context_t md_ctx;
    mbedtls_md_init(&md_ctx);
    ret = mbedtls_md_setup(&md_ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    if (ret == 0) {
        ret = mbedtls_pkcs5_pbkdf2_hmac(
            &md_ctx,
            (const unsigned char *)password, strlen(password),
            (const unsigned char *)BLAU_PBKDF2_SALT, BLAU_PBKDF2_SALT_LEN,
            BLAU_PBKDF2_ITERATIONS,
            BLAU_AES_KEY_LEN, key_out);
    }
    mbedtls_md_free(&md_ctx);
#endif

    return ret == 0;
}

/* =========================================================
 * Gestió de la clau
 * ========================================================= */
bool blau_crypto_set_key(const uint8_t key[BLAU_AES_KEY_LEN])
{
    if (_gcm_mutex == NULL) {
        _gcm_mutex = xSemaphoreCreateMutex();
        if (_gcm_mutex == NULL) return false;
    }

    _lock();
    if (_key_set) {
        mbedtls_gcm_free(&_gcm_ctx);
        _key_set = false;
    }
    mbedtls_gcm_init(&_gcm_ctx);
    int ret = mbedtls_gcm_setkey(&_gcm_ctx, MBEDTLS_CIPHER_ID_AES,
                                 key, BLAU_AES_KEY_LEN * 8u);
    if (ret == 0) {
        _key_set = true;
    } else {
        mbedtls_gcm_free(&_gcm_ctx);
    }
    _unlock();

    return _key_set;
}

bool blau_crypto_has_key(void)
{
    return _key_set;
}

void blau_crypto_clear_key(void)
{
    if (!_key_set) return;
    _lock();
    mbedtls_gcm_free(&_gcm_ctx);
    _key_set = false;
    _unlock();
}

/* =========================================================
 * blau_v2_encrypt — Mesura 2 (AES-128-GCM)
 * ========================================================= */
bool blau_v2_encrypt(const BlauPacket_t *v1pkt, uint32_t nonce, BlauPacketV2_t *out)
{
    if (!_key_set || v1pkt == NULL || out == NULL) return false;

    uint8_t iv[BLAU_GCM_IV_LEN];
    memcpy(iv, &nonce, BLAU_GCM_IV_LEN);   /* uint32 LE */

    out->version = BLAU_PROTO_VERSION_V2;
    out->nonce   = nonce;

    _lock();
    int ret = mbedtls_gcm_crypt_and_tag(
        &_gcm_ctx, MBEDTLS_GCM_ENCRYPT,
        BLAU_PLAINTEXT_LEN,
        iv, BLAU_GCM_IV_LEN,
        NULL, 0,                                   /* sense AAD */
        ((const uint8_t *)v1pkt) + 1,              /* bytes 1–8 del v1 */
        out->ciphertext,
        BLAU_GCM_TAG_LEN, out->tag);
    _unlock();

    return ret == 0;
}

/* =========================================================
 * blau_v2_decrypt — Mesura 2 (verificació + desxifrat)
 * ========================================================= */
bool blau_v2_decrypt(const BlauPacketV2_t *in, BlauPacket_t *v1out)
{
    if (!_key_set || in == NULL || v1out == NULL) return false;
    if (in->version != BLAU_PROTO_VERSION_V2) return false;

    uint32_t nonce = in->nonce;
    uint8_t  iv[BLAU_GCM_IV_LEN];
    memcpy(iv, &nonce, BLAU_GCM_IV_LEN);

    uint8_t plain[BLAU_PLAINTEXT_LEN];

    _lock();
    int ret = mbedtls_gcm_auth_decrypt(
        &_gcm_ctx,
        BLAU_PLAINTEXT_LEN,
        iv, BLAU_GCM_IV_LEN,
        NULL, 0,
        in->tag, BLAU_GCM_TAG_LEN,
        in->ciphertext, plain);
    _unlock();

    if (ret != 0) return false;                    /* tag invàlid → forgery */

    /* Reconstrucció del paquet v1 per al pipeline existent */
    v1out->version = BLAU_PROTO_VERSION;
    memcpy(((uint8_t *)v1out) + 1, plain, BLAU_PLAINTEXT_LEN);
    blau_fill_crc(v1out);
    return true;
}

/* =========================================================
 * blau_random_initial_nonce — Mesura 4
 * ========================================================= */
uint32_t blau_random_initial_nonce(void)
{
    return (esp_random() & 0x0FFFFFFFu) + 1u;
}
