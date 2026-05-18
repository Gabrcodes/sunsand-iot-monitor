% SunSand IoT Monitor
% [FILL IN: team member names + IDs]
% Microprocessor Course — Project Discussion · 2026-05-18

# Project concept

- Project idea **#4 — IoT Environmental Monitor**, extended end to end.
- A **bidirectional LoRa link** + a **WiFi-to-cloud dashboard on AWS**.
- Node reads sensors → safety rule → 433 MHz LoRa → gateway → WiFi/HTTP → AWS Flask → live web dashboard at `iot.gabr.online:8080`.
- Demonstrates: 1-wire digital, ADC, GPIO, SPI+RF, half-duplex bidirectional radio, ESP32 WiFi + HTTP client, token-secured cloud API, FSM firmware with an EVAL/ALARM branch.

# Individual contributions

| Member | Contribution |
|---|---|
| [FILL IN: name 1] | [e.g. Node A firmware + sensors + the EVAL/ALARM rule] |
| [FILL IN: name 2] | [e.g. gateway firmware + the bidirectional ACK] |
| [FILL IN: name 3] | [e.g. WiFi + cloud service + AWS deployment] |
| [FILL IN: name 4] | [e.g. dashboard + report + presentation] |

*(Replace every [FILL IN] before presenting.)*

# System block diagram

```
 [Node A]  --LoRa data-->  [Gateway]  --WiFi/HTTP-->  [AWS EC2]  --> browser
  DHT22 + photoresistor     ESP32+LoRa                 Flask :8080  iot.gabr.online
  (EVAL/ALARM bonus)  <--LoRa ACK--  + WiFi            (token-gated)
```

- LoRa hop is **bidirectional** (gateway ACKs every packet, node confirms).
- WiFi is a **non-fatal added stage** — lose it and the link still LoRa-ACKs + prints to USB.
- *(Vector diagrams are in the report PDF.)*

# Sensors used

| Component | Interface | Role |
|---|---|---|
| ESP32-WROOM ×2 | — | MCU for node + gateway |
| LoRa Ra-01 (SX1278) ×2 | SPI | 433 MHz radio (both boards) |
| DHT22 ×1 | 1-wire digital | Temp + humidity, Node A |
| Photoresistor ×1 | Analog (ADC) | Light / day-night, Node A |

# Circuit schematic (Node A)

```
            +-----------+ GPIO4   DHT22 (temp/hum, 1-wire)
   Ra-01 ---|  ESP32    |
   (SPI)    |  Node A   | GPIO34  Photoresistor AO (ADC1)
            +-----------+
```

- Ra-01 at **3.3 V** (never 5 V) + 100 nF + ~17 cm antenna wire.
- LoRa SPI: SCK 18 / MISO 19 / MOSI 23, NSS 5 / RST 27 / DIO0 26.
- Gateway = same ESP32 + Ra-01, no sensors.

# State machine — Node A (bonus + ACK)

```
 INIT -> READ -> EVAL --(hot|dark&humid)--> ALARM --.
                   \                                  \ alarm=1
                    \--(safe)--> BUILD <--------------'
                                   |
        WAIT <- WAITACK <- SEND <--'   (WAITACK listens ~600ms for ACK)
```

- **ALARM** is a real extra state (overheat OR dark+humid).
- **WAITACK** flips the radio to RX and confirms the gateway's `ACK,<seq>`.

# State machine — Gateway (bridge)

```
 INIT(LoRa+WiFi) -> LISTEN --(packet)--> PARSE -> EMIT -> LISTEN
                       \--(5s)--> STATUS --^
 EMIT = (1) LoRa ACK back  (2) USB print  (3) WiFi HTTP POST
```

- ACK is sent **before** the POST — a slow hotspot can't break the round-trip.
- WiFi non-fatal: join fails → still ACK + USB, retry on heartbeat.

# Key code — the bonus safety rule

```c
bool hot = (tempC >= TEMP_HOT_C);
bool condensation = (night == 1 && humPct >= HUM_HIGH_PCT);
state = (hot || condensation) ? S_ALARM : S_BUILD;
```

# Key code — bidirectional ACK

```c
// gateway, on each received packet:
sendAck(seq);          // LoRa TX back to node (fast, first)
Serial.println(out);   // local debug
cloudPost(out);        // WiFi HTTP POST (may block; done last)
// node, WAITACK:
if (strncmp(rx,"ACK,",4)==0 && strtoul(rx+4,0,10)==sentSeq) ackOk=1;
```

# Cloud deployment

- Flask `cloud_app.py` as a **systemd service** on AWS EC2, **port 8080**, own venv.
- Co-tenant box runs an unrelated service on 80/443 — **isolated by port**, untouched.
- One SG rule opens 8080; ingest gated by an **X-Token** secret.
- Route 53 → `iot.gabr.online`. Token + WiFi creds **gitignored** (`secrets.h`).

# Additional features (bonus marks)

1. **Bidirectional LoRa** — gateway ACKs, node confirms; ACK before cloud POST.
2. **WiFi cloud uplink** — token-secured AWS dashboard reachable anywhere.
3. **Environment-safety ALARM state** — overheat / dark+humid; LDR participates.
4. **Self-healing + graceful degradation** — faults retry; WiFi loss ≠ outage.
5. **Hardware-independent demo path** — `USE_LORA 0` → straight to USB.
6. **Secrets hygiene** — no credentials in committed code.

# Testing

- Cloud: no/bad token → 403, good token → 204, packet shows in `/data`. **Pass.**
- WiFi: 2.4 GHz scan found hotspot −30..−45 dBm; gateway joined (172.20.10.x), POST 204. **Pass.**
- Dashboard `--fake`: renders, counters advance, day/night + alarm toggle. **Pass.**
- [FILL IN: real-hardware RSSI at demo distance + sensor sanity ticks]

# Demo video

- [EMBED OR LINK the recorded demo video here.]
- Power gateway → `# WiFi OK`; power Node A → dashboard ONLINE + `ACK ok` on node serial; cover LDR → NIGHT; warm DHT22 → **ENVIRONMENT ALARM** banner; remove → clears.

# Conclusion

- Met brief (idea #4); extended to a bidirectional link + cloud dashboard with an on-board safety state.
- Clean FSM firmware, documented protocol, secrets hygiene, 6 bonus features.
- Future: TLS on the uplink, gateway→node command path on the ACK channel, more nodes (protocol namespaces by node ID).

**Thank you — questions?**
