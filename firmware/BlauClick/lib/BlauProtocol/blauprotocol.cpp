/**
 * @file    blauprotocol.cpp
 * @brief   BlauProtocol v1 — Implementació del nucli comú
 *
 * Dependències Arduino mínimes:
 *   - WiFi.h  (per blau_get_src_id)
 *
 * Sense malloc, sense String, sense heap.
 */

#include "blauprotocol.h"
#include <string.h>   /* memset */
#include <WiFi.h>     /* WiFi.macAddress(uint8_t*) */

/* =========================================================
 * blau_crc8
 *
 * CRC-8, polinomi 0x07 (x^8 + x^2 + x + 1), valor inicial 0x00.
 * Implementació bit-a-bit: determinista, sense taula de lookup,
 * ocupa ~40 bytes de flash en lloc de 256 bytes d'una taula.
 * A 240 MHz, 10 bytes triguen < 1 µs — acceptable per al nostre ús.
 * ========================================================= */
uint8_t blau_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x00u;
    for (size_t i = 0u; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0u; bit < 8u; bit++) {
            crc = (crc & 0x80u) ? ((crc << 1u) ^ 0x07u) : (crc << 1u);
        }
    }
    return crc;
}

/* =========================================================
 * blau_fill_crc
 * ========================================================= */
void blau_fill_crc(BlauPacket_t *pkt)
{
    /* CRC cobreix tots els camps excepte el propi crc8 (byte 9) */
    pkt->crc8 = blau_crc8((const uint8_t *)pkt, BLAU_PACKET_SIZE - 1u);
}

/* =========================================================
 * blau_check_crc
 * ========================================================= */
bool blau_check_crc(const BlauPacket_t *pkt)
{
    uint8_t expected = blau_crc8((const uint8_t *)pkt, BLAU_PACKET_SIZE - 1u);
    return (expected == pkt->crc8);
}

/* =========================================================
 * blau_get_src_id
 *
 * Usa la MAC STA (interfície que s'usa en mode WIFI_STA + ESP-NOW).
 * Retorna els bytes mac[4] i mac[5] com a uint16_t big-endian.
 *
 * Nota: WiFi.macAddress(buf) escriu la MAC en ordre mac[0]..mac[5]
 *       on mac[0] és el byte més significatiu.
 * ========================================================= */
uint16_t blau_get_src_id(void)
{
    uint8_t mac[6];
    WiFi.macAddress(mac);   /* MAC de la interfície STA */
    return ((uint16_t)mac[4] << 8u) | (uint16_t)mac[5];
}

/* =========================================================
 * blau_init_packet
 *
 * Posa tots els camps a zero i omple version i src_id.
 * El cridant ha d'omplir type, seq, cmd, p1/p2/p3 i
 * finalment cridar blau_fill_crc().
 * ========================================================= */
void blau_init_packet(BlauPacket_t *pkt)
{
    memset(pkt, 0x00, sizeof(BlauPacket_t));
    pkt->version = BLAU_PROTO_VERSION;
    pkt->src_id  = blau_get_src_id();
}
