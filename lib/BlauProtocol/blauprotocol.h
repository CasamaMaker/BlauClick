/**
 * @file    blauprotocol.h
 * @brief   BlauProtocol v1 — Nucli comú compartit entre BlauLink i BlauLux
 *
 * Paquet binari fix de 10 bytes transmès via ESP-NOW.
 *
 * Layout:
 *   [VER | TYPE | SEQ | SRC_ID(2B) | CMD | P1 | P2 | P3 | CRC8]
 *    0      1     2     3-4          5     6    7    8    9
 */

#ifndef BLAUPROTOCOL_H
#define BLAUPROTOCOL_H

#include <stdint.h>
#include <stddef.h>   /* size_t */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================
 * Constants del protocol
 * ========================================================= */

#define BLAU_PROTO_VERSION    0x01u
#define BLAU_PACKET_SIZE      10u     /* sizeof(BlauPacket_t) ha de ser sempre 10 */

#define BLAU_PROTO_VERSION_V2 0x02u   /* BlauProtocol v2 — paquet xifrat (blauprotocol_crypto.h) */
#define BLAU_V2_PACKET_SIZE   21u     /* sizeof(BlauPacketV2_t): ver(1)+cipher(8)+nonce(4)+tag(8) */

/* =========================================================
 * Tipus de missatge  (camp 'type')
 * ========================================================= */

#define TYPE_EVENT            0x01u   /* Event de polsador — BlauLink → BlauLux */
#define TYPE_CMD              0x02u   /* Comanda directa  — qualsevol → BlauLux */
#define TYPE_ACK              0x03u   /* Confirmació d'execució — BlauLux → BlauLink */
#define TYPE_PING             0x04u   /* Comprovació de presència */
#define TYPE_PONG             0x05u   /* Resposta al ping */
#define TYPE_STATUS_REQ       0x06u   /* Sol·licitud d'estat */
#define TYPE_STATUS_RSP       0x07u   /* Resposta d'estat — BlauLux → qualsevol */

/* =========================================================
 * Events de polsador  (camp 'cmd' quan type == TYPE_EVENT)
 * ========================================================= */

#define EVT_CLICK_1           0x11u   /* 1 clic simple */
#define EVT_CLICK_2           0x12u   /* 2 clics ràpids */
#define EVT_CLICK_3           0x13u   /* 3 clics ràpids */
#define EVT_LONG_START        0x21u   /* Inici de pulsació llarga (>BLAU_LONG_PRESS_MS) */
#define EVT_LONG_END          0x22u   /* Fi de pulsació llarga */

/* =========================================================
 * Comandes directes  (camp 'cmd' quan type == TYPE_CMD)
 *
 *  CMD_TOGGLE           p1=—    p2=—    p3=—
 *  CMD_ON               p1=—    p2=—    p3=—
 *  CMD_OFF              p1=—    p2=—    p3=—
 *  CMD_SET_BRIGHTNESS   p1=0–100(%)  p2=—  p3=—
 *  CMD_SET_RGB          p1=R    p2=G    p3=B   (0–255 cadascun)
 *  CMD_SET_CCT          p1=warm(0–100)  p2=cold(0–100)  p3=—
 *  CMD_SET_SCENE        p1=scene_id  p2=—  p3=—
 *  CMD_DIM_UP           p1=step(1–10)  p2=—  p3=—
 *  CMD_DIM_DOWN         p1=step(1–10)  p2=—  p3=—
 * ========================================================= */

#define CMD_TOGGLE            0x01u
#define CMD_ON                0x02u
#define CMD_OFF               0x03u
#define CMD_SET_BRIGHTNESS    0x04u
#define CMD_SET_RGB           0x05u
#define CMD_SET_CCT           0x06u
#define CMD_SET_SCENE         0x07u
#define CMD_DIM_UP            0x08u
#define CMD_DIM_DOWN          0x09u

/* =========================================================
 * Codis de resultat ACK  (camp 'p1' quan type == TYPE_ACK)
 *   cmd = seq del paquet confirmat
 *   p1  = un dels valors següents
 *   p2  = codi d'error opcional (0x00 si no s'usa)
 * ========================================================= */

#define ACK_OK                0x00u   /* Paquet processat correctament */
#define ACK_ERROR             0x01u   /* Error durant l'execució */
#define ACK_DUPLICATE         0x02u   /* Ignorat: duplicat detectat */
#define ACK_UNAUTHORIZED      0x03u   /* Ignorat: src_id no autoritzat */
#define ACK_BAD_VERSION       0x04u   /* Ignorat: versió de protocol incompatible */
#define ACK_BAD_CRC           0x05u   /* Ignorat: CRC incorrecte (rarament enviat) */

/* =========================================================
 * Temporització  (en ms)
 * ========================================================= */

#define BLAU_MAX_RETRIES      3u      /* Nombre màxim de reintents sense ACK */
#define BLAU_ACK_TIMEOUT_MS   50u     /* Temps d'espera de l'ACK per intent */
#define BLAU_CLICK_WINDOW_MS  400u    /* Finestra de detecció de multi-clic */
#define BLAU_LONG_PRESS_MS    800u    /* Llindar per considerar pulsació llarga */
#define BLAU_DEDUP_WINDOW_MS  2000u   /* Finestra temporal de deduplicació (Trigger) */

/* =========================================================
 * Escalabilitat
 * ========================================================= */

#define BLAU_MAX_SOURCES      8u      /* Màxim de BlauLinks que pot gestionar un Trigger */
#define BLAU_MAX_TARGETS      4u      /* Màxim de BlauLux que pot tenir un Link */

/* =========================================================
 * Estructura del paquet  — sizeof MUST == BLAU_PACKET_SIZE (10)
 * ========================================================= */

typedef struct __attribute__((packed)) {
    uint8_t  version;  /* Byte 0: versió del protocol, sempre BLAU_PROTO_VERSION */
    uint8_t  type;     /* Byte 1: tipus (TYPE_*) */
    uint8_t  seq;      /* Byte 2: número de seqüència 0–255, circular */
    uint16_t src_id;   /* Bytes 3–4: últims 2 bytes de la MAC del remitent, big-endian */
    uint8_t  cmd;      /* Byte 5: comanda (CMD_*) o event (EVT_*) segons type */
    uint8_t  p1;       /* Byte 6: paràmetre 1 (0x00 si no s'usa) */
    uint8_t  p2;       /* Byte 7: paràmetre 2 (0x00 si no s'usa) */
    uint8_t  p3;       /* Byte 8: paràmetre 3 / flags (0x00 si no s'usa) */
    uint8_t  crc8;     /* Byte 9: CRC-8 (polinomi 0x07) dels bytes 0–8 */
} BlauPacket_t;

/* Comprovació en temps de compilació */
typedef char _blau_size_check[(sizeof(BlauPacket_t) == BLAU_PACKET_SIZE) ? 1 : -1];

/* =========================================================
 * Funcions del nucli comú — implementades a blauprotocol.cpp
 * ========================================================= */

/**
 * @brief  Calcula el CRC-8 (polinomi 0x07, init 0x00) d'un buffer.
 * @param  data  Punter al primer byte
 * @param  len   Nombre de bytes a processar
 * @return CRC-8 calculat
 */
uint8_t blau_crc8(const uint8_t *data, size_t len);

/**
 * @brief  Omple el camp crc8 del paquet a partir dels bytes 0–8.
 * @param  pkt  Paquet a signar (modificat in-place)
 */
void blau_fill_crc(BlauPacket_t *pkt);

/**
 * @brief  Verifica que el CRC del paquet és correcte.
 * @param  pkt  Paquet a verificar (no modificat)
 * @return true si el CRC coincideix, false si no
 */
bool blau_check_crc(const BlauPacket_t *pkt);

/**
 * @brief  Inicialitza un paquet amb els valors per defecte:
 *           version = BLAU_PROTO_VERSION
 *           src_id  = blau_get_src_id()
 *           tots els altres camps = 0
 * @param  pkt  Paquet a inicialitzar
 */
void blau_init_packet(BlauPacket_t *pkt);

/**
 * @brief  Retorna els últims 2 bytes de la MAC WiFi STA com a src_id (big-endian).
 *         Requereix que WiFi estigui inicialitzat.
 * @return src_id de 16 bits
 */
uint16_t blau_get_src_id(void);

#ifdef __cplusplus
}
#endif

#endif /* BLAUPROTOCOL_H */
