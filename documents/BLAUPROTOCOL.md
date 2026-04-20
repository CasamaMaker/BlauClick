# BlauProtocol v1
**Protocol de comunicació ESP-NOW per al sistema BlauLink ↔ BlauTrigger**

---

## 1. Resum executiu

BlauProtocol és un protocol binari lleuger dissenyat per a la comunicació directa entre dispositius **BlauLink** (polsador alimentat per bateria) i **BlauTrigger** (controlador de càrrega) via **ESP-NOW**.

L'objectiu principal és maximitzar la fiabilitat i minimitzar el temps de ràdio activa, ja que cada mil·lisegon addicional impacta directament en l'autonomia de la bateria del BlauLink.

### Limitacions del protocol actual

| Problema | Impacte |
|---|---|
| `char topic[50]` + `char payload[50]` → 100 bytes text pla | Sobrecàrrega innecessària; el paquet òptim és <15 bytes |
| Sense versió de protocol | Impossible evolucionar sense trencar compatibilitat |
| Sense número de seqüència | Duplicats no detectables si el ràdio retransmet |
| Sense CRC propi | Corrupció de dades silenciosa (ESP-NOW té CRC de capa 2, però no protegeix el payload semàntic) |
| Sense ACK d'aplicació | BlauLink no sap si l'ordre s'ha executat, només si l'ha rebuda la capa MAC |
| Sense identificació del remitent al payload | Depèn exclusivament de la MAC ESP-NOW per filtrar l'origen |

---

## 2. Decisions de disseny

### 2.1 Per què binari i no JSON/text?
- **Mida**: 10 bytes vs ≥30 bytes. Menys temps de tx → menys consum.
- **Parsing**: operació memòria directa (`memcpy` / cast de struct) sense `String`, `atoi` ni heap allocation en microcontroladors.
- **Determinisme**: no hi ha ambigüitats de codificació ni trailing spaces.

### 2.2 Adreçament al payload vs adreçament MAC
ESP-NOW ja usa la MAC com a adreça de lliurament, però incloure els últims 2 bytes del remitent al payload permet:
- Filtrar fàcilment missatges no autoritzats sense comparar 6 bytes.
- Identificar el remitent en callbacks sense accedir al paràmetre `mac_addr` del callback.
- Logs humans llegibles de forma compacta.

### 2.3 ACK d'aplicació vs ACK de capa MAC
ESP-NOW ofereix `OnDataSent` callback amb `ESP_NOW_SEND_SUCCESS/FAIL`, que confirma lliurament a capa MAC (capa 2). Això **no garanteix** que el BlauTrigger hagi processat l'ordre (pot estar ocupat, reiniciant, etc.).

BlauProtocol afegeix un **ACK d'aplicació** opcional: el BlauTrigger respon amb un paquet `TYPE_ACK` que confirma execució. BlauLink espera aquest ACK abans de dormir; si no arriba, reintenta.

---

## 3. Estructura del paquet

El paquet té una mida fixa de **10 bytes**, sempre.

```
 Byte  0      1      2      3-4       5      6      7      8      9
      +------+------+------+--------+------+------+------+------+------+
      | VER  | TYPE | SEQ  | SRC_ID |  CMD |  P1  |  P2  |  P3  | CRC8 |
      +------+------+------+--------+------+------+------+------+------+
        1B     1B     1B     2B        1B     1B     1B     1B     1B
```

### Definició C de l'estructura

```c
typedef struct __attribute__((packed)) {
    uint8_t  version;   // Versió del protocol (0x01)
    uint8_t  type;      // Tipus de missatge (veure TYPE_*)
    uint8_t  seq;       // Número de seqüència [0–255, wraps]
    uint16_t src_id;    // Últims 2 bytes de la MAC del remitent (big-endian)
    uint8_t  cmd;       // Comanda o codi d'event (veure CMD_* / EVT_*)
    uint8_t  p1;        // Paràmetre 1
    uint8_t  p2;        // Paràmetre 2
    uint8_t  p3;        // Paràmetre 3 / flags
    uint8_t  crc8;      // CRC-8 dels bytes 0–8
} BlauPacket_t;         // sizeof = 10 bytes
```

> **Nota de padding**: l'atribut `__attribute__((packed))` és obligatori per evitar que el compilador insereixi bytes d'alineació que farien que `sizeof(BlauPacket_t) != 10`.

---

## 4. Descripció dels camps

### 4.1 `version` (byte 0)
Versió del protocol. La implementació actual és `0x01`.

Un receptor que rebi un `version` desconegut **ha de descartar** el paquet silenciosament (no enviar ACK) i loguejar-ho.

### 4.2 `type` (byte 1) — Tipus de missatge

| Constant | Valor | Descripció | Remitent |
|---|---|---|---|
| `TYPE_EVENT` | `0x01` | Event de polsador (clic, longpress…) | BlauLink |
| `TYPE_CMD` | `0x02` | Comanda directa (control de llum) | BlauLink o futur hub |
| `TYPE_ACK` | `0x03` | Confirmació d'execució | BlauTrigger |
| `TYPE_PING` | `0x04` | Comprovació de presència | Qualsevol |
| `TYPE_PONG` | `0x05` | Resposta al ping | Qualsevol |
| `TYPE_STATUS_REQ` | `0x06` | Sol·licitud d'estat del BlauTrigger | Qualsevol |
| `TYPE_STATUS_RSP` | `0x07` | Resposta amb estat actual | BlauTrigger |

### 4.3 `seq` (byte 2) — Número de seqüència

Comptador incremental de 0 a 255 (circular). El BlauLink l'incrementa en cada nou event; els reintents del **mateix** event reutilitzen el mateix `seq`.

El BlauTrigger guarda l'últim `seq` rebut per `src_id` i **descarta duplicats** (paquet amb el mateix `seq` i `src_id` rebut dins d'una finestra de 2 segons).

### 4.4 `src_id` (bytes 3–4) — Identificador del remitent

Els últims 2 bytes de la MAC WiFi del remitent, en ordre big-endian.

```c
// Exemple d'extracció
uint8_t mac[6];
WiFi.macAddress(mac);
uint16_t src_id = ((uint16_t)mac[4] << 8) | mac[5];
```

Permet al BlauTrigger gestionar múltiples BlauLinks sense comparar MACs completes.

### 4.5 `cmd` (byte 5) — Comanda o codi d'event

El significat depèn del camp `type`:

**Quan `type == TYPE_EVENT`** — Codi d'event del polsador:

| Constant | Valor | Descripció |
|---|---|---|
| `EVT_CLICK_1` | `0x11` | 1 clic simple |
| `EVT_CLICK_2` | `0x12` | 2 clics ràpids |
| `EVT_CLICK_3` | `0x13` | 3 clics ràpids |
| `EVT_LONG_START` | `0x21` | Inici de pulsació llarga (>1s) |
| `EVT_LONG_END` | `0x22` | Fi de pulsació llarga |

El BlauTrigger **decideix localment** quina acció fer per a cada event (configurable per EEPROM). Aquesta separació manté el BlauLink simple i agnòstic del tipus de càrrega.

**Quan `type == TYPE_CMD`** — Comanda directa de control:

| Constant | Valor | `p1` | `p2` | `p3` | Descripció |
|---|---|---|---|---|---|
| `CMD_TOGGLE` | `0x01` | — | — | — | Canvia estat on↔off |
| `CMD_ON` | `0x02` | — | — | — | Encendre |
| `CMD_OFF` | `0x03` | — | — | — | Apagar |
| `CMD_SET_BRIGHTNESS` | `0x04` | `0–100` (%) | — | — | Fixa brillantor |
| `CMD_SET_RGB` | `0x05` | R `0–255` | G `0–255` | B `0–255` | Color RGB |
| `CMD_SET_CCT` | `0x06` | Warm `0–100` | Cold `0–100` | — | Temperatura de color WW/CW |
| `CMD_SET_SCENE` | `0x07` | scene_id | — | — | Activar escena predefinida |
| `CMD_DIM_UP` | `0x08` | step `1–10` | — | — | Pujar brillantor N punts |
| `CMD_DIM_DOWN` | `0x09` | step `1–10` | — | — | Baixar brillantor N punts |

**Quan `type == TYPE_ACK`:**

| Camp | Valor |
|---|---|
| `cmd` | `seq` del paquet confirmat |
| `p1` | `0x00` = OK, `0x01` = Error, `0x02` = Ignorat (duplicat) |
| `p2` | Codi d'error opcional |

**Quan `type == TYPE_STATUS_RSP`:**

| Camp | Contingut |
|---|---|
| `p1` | Estat actual: bit 0 = on/off |
| `p2` | Brillantor actual `0–100` |
| `p3` | `control_type` configurat (0=relay, 1=digled, 2=pwm, 3=cct…) |

### 4.6 `p1`, `p2`, `p3` (bytes 6–8) — Paràmetres

Paràmetres específics de cada comanda. Si una comanda no usa un paràmetre, el camp ha de ser `0x00`.

### 4.7 `crc8` (byte 9) — Checksum

CRC-8 (polinomi `0x07`, valor inicial `0x00`) calculat sobre els bytes 0–8 (tots els camps excepte el propi CRC).

```c
uint8_t crc8_calc(const uint8_t *data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
        }
    }
    return crc;
}

// Ús:
BlauPacket_t pkt = { ... };
pkt.crc8 = crc8_calc((uint8_t*)&pkt, sizeof(pkt) - 1);

// Verificació al receptor:
bool valid = (crc8_calc((uint8_t*)&pkt, sizeof(pkt) - 1) == pkt.crc8);
```

---

## 5. Flux de comunicació

### 5.1 Flux normal: 1 clic → toggle

```
BlauLink                                    BlauTrigger
    |                                            |
    |  [Desperta de deep sleep]                  |
    |  [Detecta 1 clic]                          |
    |                                            |
    |--- TYPE_EVENT, EVT_CLICK_1, seq=N -------> |
    |                                            |  [Executa acció: toggle]
    |<-- TYPE_ACK, cmd=N, p1=OK --------------- |
    |                                            |
    |  [ACK rebut → va a dormir]                 |
    |                                            |
```

**Temporització:**
- Tx BlauLink → Rx BlauTrigger: ~1–3 ms
- Processament BlauTrigger + Tx ACK: ~5–10 ms
- Temps total awake BlauLink (mínim): ~15–25 ms

### 5.2 Flux amb reintents (no ACK rebut)

```
BlauLink                                    BlauTrigger
    |                                            |
    |--- TYPE_EVENT, EVT_CLICK_1, seq=N -------> |  (perdut)
    |                                            |
    |   [Timeout 50ms, reintent 1]               |
    |--- TYPE_EVENT, EVT_CLICK_1, seq=N -------> |
    |                                            |  [Executa acció]
    |<-- TYPE_ACK, cmd=N, p1=OK --------------- |
    |                                            |
    |  [ACK rebut → va a dormir]                 |
    |                                            |
```

El BlauTrigger detecta el duplicat (`seq=N` ja processat) però envia ACK igualment perquè el BlauLink no sap que el primer ha arribat.

### 5.3 Flux amb parell de clics (doble clic)

```
BlauLink                                    BlauTrigger
    |                                            |
    |  [Detecta 2 clics dins 400ms]              |
    |--- TYPE_EVENT, EVT_CLICK_2, seq=N+1 -----> |
    |                                            |  [Executa acció per CLICK_2]
    |<-- TYPE_ACK, cmd=N+1, p1=OK ------------- |
    |                                            |
```

### 5.4 Flux de ping (comprovació de presència)

```
BlauLink                                    BlauTrigger
    |                                            |
    |--- TYPE_PING, cmd=0x00, seq=N -----------> |
    |<-- TYPE_PONG, cmd=0x00, seq=N ----------- |
    |                                            |
```

---

## 6. Mecanismes de fiabilitat

### 6.1 Reintents al BlauLink

```c
#define BLAU_MAX_RETRIES     3
#define BLAU_ACK_TIMEOUT_MS  50

bool sendWithAck(BlauPacket_t *pkt) {
    for (int i = 0; i < BLAU_MAX_RETRIES; i++) {
        esp_now_send(receiverMac, (uint8_t*)pkt, sizeof(BlauPacket_t));
        uint32_t t = millis();
        while (millis() - t < BLAU_ACK_TIMEOUT_MS) {
            if (ackReceived && ackSeq == pkt->seq) return true;
            delay(1);
        }
    }
    return false;  // Fallida definitiva
}
```

### 6.2 Deduplicació al BlauTrigger

```c
#define MAX_SOURCES 8

typedef struct {
    uint16_t src_id;
    uint8_t  last_seq;
    uint32_t last_time_ms;
} SourceRecord_t;

SourceRecord_t sources[MAX_SOURCES];

#define DEDUP_WINDOW_MS 2000

bool isDuplicate(uint16_t src_id, uint8_t seq) {
    for (int i = 0; i < MAX_SOURCES; i++) {
        if (sources[i].src_id == src_id) {
            if (sources[i].last_seq == seq &&
                (millis() - sources[i].last_time_ms) < DEDUP_WINDOW_MS) {
                return true;
            }
            sources[i].last_seq = seq;
            sources[i].last_time_ms = millis();
            return false;
        }
    }
    // Font nova: afegir
    // ... (gestió de l'array)
    return false;
}
```

### 6.3 Validació de CRC al receptor

El receptor (BlauTrigger i BlauLink) **sempre** verifica el CRC8 com a primer pas. Si el CRC no coincideix, el paquet es descarta silenciosament (sense enviar ACK ni log d'error actiu).

### 6.4 Filtre de versió

Si `pkt.version != BLAU_PROTO_VERSION`, descartar. Permet desplegar firmwares nous sense causar comportament imprevisible en dispositius amb firmware antic.

---

## 7. Consideracions d'eficiència energètica

El BlauLink és el dispositiu crític en consum. El temps de ràdio activa és proporcional a la vida de la bateria.

### Estratègia recomanada per al BlauLink

```
Boot
 └─> Detectar tipus d'event (clic/longpress)   [~5ms]
 └─> Iniciar WiFi STA + ESP-NOW               [~10ms]
 └─> Enviar paquet + esperar ACK              [~15–50ms]
 └─> Si ACK OK → deep sleep
     Si ACK KO → reintent (màx. 3) → deep sleep
```

**Temps total d'activitat per clic únic amb ACK:** ~30–80 ms
**Temps total sense ACK (3 reintents fallits):** ~160 ms

### Paràmetres de temporització recomanats

| Paràmetre | Valor | Justificació |
|---|---|---|
| `ACK_TIMEOUT_MS` | 50 ms | Suficient per a temps de processament del BlauTrigger |
| `MAX_RETRIES` | 3 | Equilibri entre fiabilitat i consum |
| `CLICK_WINDOW_MS` | 400 ms | Finestra de detecció de multi-clic |
| `LONG_PRESS_MS` | 800 ms | Umbral de pulsació llarga |

### Canal WiFi fix

El Blautrigger hauria d'operar sempre al **mateix canal WiFi** (p.ex. canal 1). Això evita que el BlauLink hagi d'escanejar canals per trobar el receptor, reduint significativament el temps d'inicialització.

```c
// BlauTrigger: forçar canal 1
WiFi.softAP(ssid, "", 1);  // tercer paràmetre = canal

// BlauLink: configurar ESP-NOW al canal del peer
esp_now_peer_info_t peer;
peer.channel = 1;  // Ha de coincidir amb el BlauTrigger
```

---

## 8. Escalabilitat: múltiples BlauLinks → múltiples BlauTriggers

### 8.1 Vinculació N:1 (múltiples botons, un receptor)

El BlauTrigger manté una taula de `src_id` autoritzats a la EEPROM. Qualsevol BlauLink de la taula pot controlar-lo.

```c
// EEPROM BlauTrigger: llista de src_ids autoritzats
uint16_t authorizedSources[8];
```

### 8.2 Vinculació 1:N (un botó, múltiples receptors)

El BlauLink emmagatzema múltiples MACs de destinatari. En polsar, envia el paquet a cadascuna seqüencialment i espera ACK de totes.

```c
// EEPROM BlauLink
uint8_t receiverMacs[4][6];  // Fins a 4 receptors
```

### 8.3 Topologia multicast

ESP-NOW suporta enviar a la **MAC de broadcast** (`FF:FF:FF:FF:FF:FF`). Útil per a escenaris on un botó ha de notificar tots els BlauTriggers de la xarxa simultàniament. En broadcast no hi ha ACK de capa MAC, per la qual cosa s'han de fer més reintents o acceptar-ne la pèrdua.

```c
// dst_id = 0xFFFF al payload indica que és un broadcast
uint8_t broadcastMac[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
```

---

## 9. Mapa de migració des del codi actual

Per migrar des de l'estructura `struct_message` actual:

| Codi actual | BlauProtocol v1 |
|---|---|
| `topic = "llum"`, `payload = "conmuta"` | `type=TYPE_EVENT`, `cmd=EVT_CLICK_1` |
| `topic = "digled"`, `payload = color` | `type=TYPE_CMD`, `cmd=CMD_SET_RGB`, `p1=R`, `p2=G`, `p3=B` |
| `topic = "dimmer"`, `payload = %` | `type=TYPE_CMD`, `cmd=CMD_SET_BRIGHTNESS`, `p1=%` |
| `topic = "wwcw"`, `payload = %&%` | `type=TYPE_CMD`, `cmd=CMD_SET_CCT`, `p1=warm`, `p2=cold` |
| Sense ACK d'aplicació | `TYPE_ACK` amb `seq` confirmat |

---

## 10. Possibles ampliacions futures

| Funcionalitat | Mecanisme |
|---|---|
| **Escenes** | `TYPE_CMD` + `CMD_SET_SCENE` + `p1=scene_id` |
| **Dimming continu** (mantenir premut) | `EVT_LONG_START` / `EVT_LONG_END` + `CMD_DIM_UP/DOWN` |
| **Control de grup** | `dst_id = 0xFFFF` broadcast + filtre de grup a `p3` |
| **Seguretat / xifratge** | Afegir camp `nonce` de 4 bytes i xifrar `cmd+p1+p2+p3` amb AES-128-CCM |
| **OTA trigger** | Nou `TYPE_OTA_NOTIFY` per notificar disponibilitat de firmware |
| **Telemetria de bateria** | Camp `p3` del paquet `TYPE_EVENT` = nivell de bateria `0–100` |
| **Feedback hàptic/visual** | `TYPE_CMD` + `CMD_ACK_BLINK` des del BlauTrigger cap al BlauLink |
| **Hub MQTT** | Passarel·la que escolta ESP-NOW i re-publica via MQTT; el protocol no canvia |

---

## 11. Constants de referència (header C)

```c
// blauprotocol.h

#ifndef BLAUPROTOCOL_H
#define BLAUPROTOCOL_H

#include <stdint.h>

#define BLAU_PROTO_VERSION   0x01
#define BLAU_PACKET_SIZE     10

// --- Tipus de missatge ---
#define TYPE_EVENT           0x01
#define TYPE_CMD             0x02
#define TYPE_ACK             0x03
#define TYPE_PING            0x04
#define TYPE_PONG            0x05
#define TYPE_STATUS_REQ      0x06
#define TYPE_STATUS_RSP      0x07

// --- Events de polsador ---
#define EVT_CLICK_1          0x11
#define EVT_CLICK_2          0x12
#define EVT_CLICK_3          0x13
#define EVT_LONG_START       0x21
#define EVT_LONG_END         0x22

// --- Comandes directes ---
#define CMD_TOGGLE           0x01
#define CMD_ON               0x02
#define CMD_OFF              0x03
#define CMD_SET_BRIGHTNESS   0x04
#define CMD_SET_RGB          0x05
#define CMD_SET_CCT          0x06
#define CMD_SET_SCENE        0x07
#define CMD_DIM_UP           0x08
#define CMD_DIM_DOWN         0x09

// --- Codis ACK (camp p1) ---
#define ACK_OK               0x00
#define ACK_ERROR            0x01
#define ACK_DUPLICATE        0x02
#define ACK_UNAUTHORIZED     0x03

// --- Temporització ---
#define BLAU_MAX_RETRIES     3
#define BLAU_ACK_TIMEOUT_MS  50
#define BLAU_CLICK_WINDOW_MS 400
#define BLAU_LONG_PRESS_MS   800

typedef struct __attribute__((packed)) {
    uint8_t  version;
    uint8_t  type;
    uint8_t  seq;
    uint16_t src_id;
    uint8_t  cmd;
    uint8_t  p1;
    uint8_t  p2;
    uint8_t  p3;
    uint8_t  crc8;
} BlauPacket_t;

#endif // BLAUPROTOCOL_H
```

---

*BlauProtocol v1 — Documentació generada per al projecte BlauLink/BlauTrigger*
