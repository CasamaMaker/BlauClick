# Coding Guidelines
## BlauLink / BlauTrigger — ESP32-C3 Arduino Project

> These guidelines apply to both **BlauLink** (battery sender) and **BlauTrigger** (AC receiver).  
> They are written for AI agents and human developers alike.

---

## 1. General Principles

- **Reliability over performance.** A missed event is worse than a 10ms delay.
- **Deterministic behavior.** Avoid dynamic allocation (`new`, `malloc`) in runtime paths.
- **Minimal footprint.** Every byte of RAM and every millisecond of active time matters.
- **No partial implementations.** Either implement a feature fully or leave it unstarted with a `// TODO` comment. Never leave broken stubs.
- **Do not invent behavior.** If a requirement is unclear, do not guess — ask or leave a `// TODO`.

---

## 2. Architecture Rules

- **BlauLink** is always the initiator (sender). It sends TYPE_EVENT or TYPE_CMD and expects an ACK.
- **BlauTrigger** is always the responder. It receives, deduplicates, acts, and sends an ACK from `loop()` — never from the ISR callback.
- **Callbacks (`OnDataRecv`, `OnDataSent`) MUST NOT** perform any blocking operations, allocations, or slow I/O. Set flags or copy data only.
- **ACK responses** MUST be queued (`_ack_pending = true`) and sent from `loop()`, never from within `OnDataRecv`.
- **The BlauProtocol library** (`lib/BlauProtocol/`) is shared between both devices. Changes to it affect both — treat it as a stable API.
- **Hardware variants** (BLAULINK_V1, BLAULINK_V2, SONOFF_BASIC_R4) are resolved at compile time via `#ifdef`. Do not add runtime hardware detection.
- **WiFi mode**: BlauLink uses `WIFI_STA`; BlauTrigger uses `WIFI_AP`. Never switch modes at runtime.

---

## 3. File Structure

```
src/
  main.cpp              — Application logic (setup, loop, callbacks, web server)

lib/BlauProtocol/
  blauprotocol.h        — Packet struct, all constants (TYPE_*, CMD_*, EVT_*, ACK_*, BLAU_*)
  blauprotocol.cpp      — CRC-8, blau_init_packet(), blau_check_crc(), blau_fill_crc()
  blauprotocol_link.h   — BlauLink-side helpers: build packets, check ACK
  blauprotocol_trg.h    — BlauTrigger-side helpers: parse, deduplicate, build ACK/PONG/STATUS

data/
  index.html            — Captive portal UI
  style.css             — UI styles

platformio.ini          — Build config, board, lib_deps
```

- **MUST NOT** add new source files to `src/` without a clear functional reason.
- **MUST NOT** add runtime dependencies not already in `platformio.ini` without explicit approval.
- Protocol constants and packet definitions belong **only** in `blauprotocol.h`.

---

## 4. Naming Conventions

| Prefix / Pattern | Scope | Examples |
|---|---|---|
| `blau_` | BlauProtocol public API functions | `blau_crc8()`, `blau_parse_packet()`, `blau_build_ack()` |
| `BLAU_` | Protocol constants and limits | `BLAU_MAX_RETRIES`, `BLAU_ACK_TIMEOUT_MS`, `BLAU_PACKET_SIZE` |
| `TYPE_` | Packet type codes | `TYPE_EVENT`, `TYPE_ACK`, `TYPE_PING` |
| `CMD_` | Command codes (cmd field) | `CMD_TOGGLE`, `CMD_SET_BRIGHTNESS`, `CMD_SET_RGB` |
| `EVT_` | Event codes (cmd field in TYPE_EVENT) | `EVT_CLICK_1`, `EVT_LONG_START` |
| `ACK_` | ACK status codes (p1 field in TYPE_ACK) | `ACK_OK`, `ACK_DUPLICATE`, `ACK_BAD_CRC` |
| `_ack_*` | Volatile ACK state flags (BlauLink ISR side) | `_ack_received`, `_ack_seq`, `_ack_result` |
| `_*_pending` | Async operation flag in main loop | `_ack_pending` |
| `BLAULINK_V1/V2`, `SONOFF_BASIC_R4` | Compile-time hardware variant macros | `#ifdef BLAULINK_V2` |

- **Variable and function names**: camelCase for local variables and functions.
- **Constants and macros**: SCREAMING_SNAKE_CASE.
- **Structs**: PascalCase with `_t` suffix (e.g., `BlauPacket_t`).
- **Comments and UI strings**: Written in **Catalan**. Do not change this to English or Spanish.
- Do **NOT** reuse a `TYPE_*`, `CMD_*`, or `EVT_*` constant for a different semantic purpose.

---

## 5. Embedded Constraints (ESP32-C3 Specific)

- **Stack size**: Default Arduino stack is ~8KB. Avoid deep recursion or large stack allocations.
- **No dynamic allocation** at runtime: no `new`, no `malloc`, no `std::vector` after `setup()`.  
  Use fixed-size arrays and static buffers.
- **Volatile for ISR-shared variables**: Any variable written in a callback (`OnDataRecv`, `OnDataSent`) and read in `loop()` or `setup()` MUST be declared `volatile`.

```cpp
// CORRECT
volatile bool _ack_received = false;
volatile uint8_t _ack_seq = 0;

// WRONG — race condition
bool _ack_received = false;
```

- **ADC on ESP32-C3**: Enable/disable the voltage divider via the enable pin before and after every ADC read. Average multiple samples (minimum 5, currently 10).
- **LittleFS**: Always check `LittleFS.begin()` return value before serving files.
- **`esp_deep_sleep_start()`**: Called only from BlauLink, never from BlauTrigger.
- **`ledcSetup()` / `ledcAttachPin()`**: Required for PWM on ESP32-C3. Do not use `analogWrite()`.
- **`esp_now_send()`**: Returns immediately. Delivery confirmation comes via `OnDataSent` callback — do not assume success from the return value alone.
- **Watchdog**: Do not use blocking `while(true)` without `yield()` or `delay()` calls; this will trigger the hardware watchdog.

---

## 6. Communication Protocol Rules

### Packet Structure (10 bytes, fixed)

```c
typedef struct __attribute__((packed)) {
    uint8_t  version;  // Always BLAU_PROTO_VERSION (0x01)
    uint8_t  type;     // TYPE_*
    uint8_t  seq;      // Circular sequence number 0–255
    uint16_t src_id;   // Last 2 bytes of sender MAC (big-endian)
    uint8_t  cmd;      // CMD_* or EVT_* depending on type
    uint8_t  p1;       // Parameter 1 (or ACK status code if TYPE_ACK)
    uint8_t  p2;       // Parameter 2
    uint8_t  p3;       // Parameter 3 / flags
    uint8_t  crc8;     // CRC-8, poly 0x07, init 0x00, over bytes 0–8
} BlauPacket_t;
```

- **MUST** always call `blau_fill_crc()` before sending any packet.
- **MUST** always call `blau_check_crc()` and verify `version == BLAU_PROTO_VERSION` before processing any received packet.
- **MUST NOT** send packets larger or smaller than `BLAU_PACKET_SIZE` (10 bytes).
- **MUST NOT** modify the BlauPacket_t struct layout. It is a wire format.

### Sequence Numbers

- BlauLink generates: `blau_seq = millis() & 0xFF` — assigns a new seq per transmission session (not per retry).
- Retries of the same event **MUST** reuse the same seq number. This is critical for deduplication.
- BlauTrigger returns the confirmed seq in the `cmd` field of the ACK packet.
- BlauLink validates ACK with `blau_is_ack_for(&pkt, seq)`.

### Retry and ACK Strategy

```
for attempt in 0..BLAU_MAX_RETRIES-1:
    send packet (same seq each retry)
    wait up to BLAU_ACK_TIMEOUT_MS for matching ACK
    if ACK_OK or ACK_DUPLICATE → success, stop
    if ACK_ERROR → continue to next attempt
return false on exhaustion
```

- **MUST NOT** increase `BLAU_MAX_RETRIES` beyond 3 or `BLAU_ACK_TIMEOUT_MS` beyond 50ms without power budget analysis. These directly impact battery life.
- **ACK_DUPLICATE is a success**: the action was already executed. BlauLink MUST treat it as `ACK_OK`.

### Deduplication (BlauTrigger)

- BlauTrigger maintains a table of `(src_id, last_seq, timestamp)` for up to `BLAU_MAX_SOURCES` (8) senders.
- Same `(src_id, seq)` pair within `BLAU_DEDUP_WINDOW_MS` (2000ms) → send `ACK_DUPLICATE`, do NOT execute action.
- When the table is full, the oldest entry is evicted (LRU).
- **MUST NOT** extend `BLAU_DEDUP_WINDOW_MS` beyond what the BlauLink retry window covers.

### Adding New Commands

1. Define the constant in `blauprotocol.h` under `CMD_*` or `EVT_*`.
2. Add the handler in BlauTrigger's `OnDataRecv()` switch/case.
3. If the command has parameters, document the `p1`/`p2`/`p3` meaning in `blauprotocol.h`.
4. If unimplemented, send `ACK_ERROR` and log `"CMD no implementat: 0x..."`.

---

## 7. Error Handling & Logging

- **Packet errors are silent drops.** A failed `blau_parse_packet()` (bad CRC, wrong version, wrong size) results in no action and no ACK.
- **Unknown commands** MUST respond with `ACK_ERROR` — do not silently ignore them.
- **ESP-NOW initialization failure** (`esp_now_init()`) MUST trigger a device restart (`ESP.restart()`).
- **LittleFS failure** on web server setup: log the error and return early. Do not crash.
- **EEPROM MAC validation**: if all bytes are `0xFF`, the MAC is unset. Do not attempt ESP-NOW pairing.

### Logging Format

```cpp
Serial.printf("[BlauTrigger] OnDataRecv: from %02X:%02X, seq=%d, type=0x%02X\n",
              mac[4], mac[5], pkt.seq, pkt.type);
```

- Use `Serial.printf()` for all debug output. Prefix with `[BlauLink]` or `[BlauTrigger]`.
- **MUST NOT** leave `Serial.print()` calls in hot paths (e.g., inside retry loops or ISR callbacks) in production builds.
- Use `#ifdef DEBUG` guards if adding verbose logging.

---

## 8. Power Efficiency Rules

### BlauLink (Battery-Powered)

- **MUST** call `esp_deep_sleep_start()` after every transmission attempt (success or failure).
- **MUST NOT** add any `delay()` calls longer than 10ms between wake-up and sleep.
- **MUST NOT** leave WiFi or ESP-NOW active after the transmission sequence completes.
- The ADC enable pin (`enVBatterySense`) MUST be driven LOW before and after each ADC read session.
- AP config mode MUST auto-exit after 60 seconds (`CONFIG_TIMEOUT_MS`).
- **MUST NOT** initialize FastLED, open LittleFS, or start a web server unless that code path is actually needed (AP mode only).

### BlauTrigger (AC-Powered)

- No deep sleep. Loop runs continuously.
- **MUST NOT** use `delay()` in `loop()`. Use non-blocking timing with `millis()`.
- LED feedback (FastLED) MUST be brief (< 500ms). Do not hold the LED active indefinitely.

---

## 9. Code Style

- **Indentation**: 4 spaces. No tabs.
- **Braces**: Always use braces for `if`/`for`/`while`, even single-line bodies.
- **`#define` vs `const`**: Use `const uint8_t` or `constexpr` for typed constants inside `.cpp`/`.h`. Reserve `#define` for compile-time feature flags (hardware variants).
- **Function length**: Keep functions under 60 lines. Extract sub-tasks if needed.
- **No magic numbers**: Every numeric literal used in protocol logic MUST be a named constant from `blauprotocol.h`.

```cpp
// WRONG
if (pkt.version != 0x01) return false;

// CORRECT
if (pkt.version != BLAU_PROTO_VERSION) return false;
```

- **`__attribute__((packed))`**: Required on `BlauPacket_t`. Do not remove it.
- **Language**: Comments, variable names, and UI strings are in **Catalan**. Do not translate.
- **No `String` class** in protocol or callback code. Use `char[]` and `snprintf()`.
- **`volatile`** on all ISR-shared variables (see Section 5).

---

## 10. Testing & Debugging

- **Serial monitor**: 115200 baud. Always include log lines at send, receive, ACK, and error points.
- **Smoke test after changes**:
  1. Flash BlauLink → press button → BlauTrigger toggles output → LED turns green on BlauLink.
  2. Repeat 5 times quickly → no duplicate actions on BlauTrigger.
  3. Power-cycle BlauTrigger → BlauLink can still pair and trigger.
- **Duplicate detection test**: Send the same `(src_id, seq)` twice within 2 seconds. BlauTrigger MUST send `ACK_DUPLICATE` the second time and NOT toggle the output.
- **CRC test**: Corrupt one byte in a packet. BlauTrigger MUST silently drop it.
- **No unit test framework is in place.** Tests are manual via Serial logs and hardware observation.
- **Simulating deep sleep during development**: Comment out `esp_deep_sleep_start()` temporarily. Revert before committing.

---

## 11. AI Instructions

> These rules apply to any AI agent (Claude Code or similar) generating or modifying code in this project.

### Scope of Changes

- **Modify only what is explicitly requested.** Do not refactor, rename, or restructure surrounding code.
- **Do not rewrite full files** unless explicitly instructed. Prefer surgical edits.
- **Do not add features** that were not requested, even if they seem useful.
- **Do not remove existing comments** unless they are factually wrong.

### Compatibility

- **Preserve the BlauPacket_t wire format.** Any change to field order, size, or type breaks communication between devices.
- **Preserve existing constant values** in `TYPE_*`, `CMD_*`, `EVT_*`, `ACK_*`. Renaming is safe; changing the numeric value is not.
- **Keep the shared BlauProtocol library identical** between BlauLink and BlauTrigger. If you change it in one, note that the other must be updated too.
- **Do not change hardware pin assignments** unless explicitly asked. These are validated against physical PCB layouts.

### Protocol Rules (Non-Negotiable)

- **Never send a packet without calling `blau_fill_crc()` first.**
- **Never process a received packet without calling `blau_check_crc()` first.**
- **Retries MUST use the same `seq` number.** Do not increment seq on retry.
- **ACK_DUPLICATE MUST be treated as success** on the sender side.
- **ACK responses MUST be sent from `loop()`, not from `OnDataRecv()`.**

### Style Compliance

- **Comments in Catalan.** Do not write English comments unless explicitly asked.
- **No dynamic allocation** (`new`, `malloc`, `std::vector`) in runtime paths.
- **No `String` class** in protocol or callback code.
- **No `delay()`** in `loop()` on BlauTrigger.
- **All ISR-shared variables must be `volatile`.**

### When Uncertain

- **Do not guess** at intent or behavior. Leave a `// TODO: [pregunta]` comment and explain the ambiguity in your response.
- **Do not assume** a command is implemented if it is not present in the switch/case. Unimplemented commands MUST respond with `ACK_ERROR`.
- **Do not assume** pin numbers. Always reference the hardware variant macros (`#ifdef BLAULINK_V2`, etc.) or explicitly ask.
- **If a change would affect the wire protocol**, stop and ask before implementing.

### Summary Checklist (Before Submitting Code)

- [ ] Only modified what was requested
- [ ] BlauPacket_t struct unchanged
- [ ] CRC filled before send, checked on receive
- [ ] Same seq used across retries
- [ ] ISR callbacks do no blocking work
- [ ] ACK sent from loop(), not from callback
- [ ] No dynamic allocation
- [ ] No magic numbers — named constants used
- [ ] Comments in Catalan
- [ ] Both BlauLink and BlauTrigger affected if BlauProtocol was changed
