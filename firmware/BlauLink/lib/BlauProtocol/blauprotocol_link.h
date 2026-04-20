/**
 * @file    blauprotocol_link.h
 * @brief   BlauProtocol v1 — Funcions d'ajuda per al costat emissor (BlauLink)
 *
 * Inclou NOMÉS la lògica de construcció de paquets.
 * NO inclou:
 *   - Lògica de reintents
 *   - Crida a esp_now_send()
 *   - Lògica de deep sleep
 *
 * Ús típic al BlauLink:
 *   BlauPacket_t pkt;
 *   blau_build_event_packet(&pkt, seq, EVT_CLICK_1);
 *   // → esp_now_send(receiverMac, (uint8_t*)&pkt, sizeof(pkt));
 */

#ifndef BLAUPROTOCOL_LINK_H
#define BLAUPROTOCOL_LINK_H

#include "blauprotocol.h"

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
 * @param event  Codi d'event: EVT_CLICK_1 / EVT_CLICK_2 / EVT_CLICK_3
 *                             EVT_LONG_START / EVT_LONG_END
 *
 * Nota sobre p3 i telemetria de bateria (futur):
 *   p3 es reserva per al nivell de bateria (0–100). Mentre no
 *   s'implementi, es posa a 0x00. Quan s'afegeixi, el BlauTrigger
 *   podrà loguejar-lo sense canviar la interfície.
 * ========================================================= */
static inline void blau_build_event_packet(BlauPacket_t *pkt,
                                            uint8_t       seq,
                                            uint8_t       event)
{
    blau_init_packet(pkt);   /* version, src_id, zeros */
    pkt->type = TYPE_EVENT;
    pkt->seq  = seq;
    pkt->cmd  = event;
    pkt->p1   = 0x00u;
    pkt->p2   = 0x00u;
    pkt->p3   = 0x00u;       /* reservat: nivell bateria (futur) */
    blau_fill_crc(pkt);
}

/* =========================================================
 * blau_build_cmd_packet
 *
 * Construeix un paquet TYPE_CMD per enviar una comanda directa.
 *
 * @param pkt  Paquet de sortida
 * @param seq  Número de seqüència
 * @param cmd  Comanda: CMD_TOGGLE / CMD_ON / CMD_OFF /
 *                      CMD_SET_BRIGHTNESS / CMD_SET_RGB /
 *                      CMD_SET_CCT / CMD_SET_SCENE /
 *                      CMD_DIM_UP / CMD_DIM_DOWN
 * @param p1   Paràmetre 1 (0x00 si no s'usa)
 * @param p2   Paràmetre 2 (0x00 si no s'usa)
 * @param p3   Paràmetre 3 (0x00 si no s'usa)
 *
 * Exemples:
 *   Toggle:          blau_build_cmd_packet(&pkt, seq, CMD_TOGGLE, 0,0,0)
 *   Brillantor 75%:  blau_build_cmd_packet(&pkt, seq, CMD_SET_BRIGHTNESS, 75,0,0)
 *   Color vermell:   blau_build_cmd_packet(&pkt, seq, CMD_SET_RGB, 255,0,0)
 *   CCT warm 80%:    blau_build_cmd_packet(&pkt, seq, CMD_SET_CCT, 80,20,0)
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
 *
 * Construeix un paquet TYPE_PING per comprovar presència del Trigger.
 *
 * @param pkt  Paquet de sortida
 * @param seq  Número de seqüència
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
 *
 * Demana l'estat actual al BlauTrigger (on/off, brillantor, tipus).
 *
 * @param pkt  Paquet de sortida
 * @param seq  Número de seqüència
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
 * Ha de cridar-se dins del callback OnDataRecv del BlauLink
 * per saber si el Trigger ha confirmat el paquet enviat.
 *
 * @param  received  Paquet rebut (ja verificat en CRC i versió)
 * @param  seq       Número de seqüència que s'espera confirmar
 * @return true si és un ACK per a 'seq', false altrament
 *
 * Nota: NO comprova CRC ni versió; es pressuposa que el cridant
 *       ja ha passat el paquet per blau_check_crc().
 * ========================================================= */
static inline bool blau_is_ack_for(const BlauPacket_t *received, uint8_t seq)
{
    return (received->type == TYPE_ACK) && (received->cmd == seq);
}

/* =========================================================
 * blau_ack_status
 *
 * Extreu el codi de resultat d'un paquet ACK (camp p1).
 * Útil per distingir ACK_OK de ACK_DUPLICATE, ACK_ERROR, etc.
 *
 * @param  ack  Paquet de tipus TYPE_ACK
 * @return codi ACK_* (p1)
 * ========================================================= */
static inline uint8_t blau_ack_status(const BlauPacket_t *ack)
{
    return ack->p1;
}

#ifdef __cplusplus
}
#endif

#endif /* BLAUPROTOCOL_LINK_H */
