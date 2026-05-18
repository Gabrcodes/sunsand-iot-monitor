# SunSand IoT Monitor

Microprocessor course project — a **bidirectional LoRa sensor link** that
bridges over WiFi to a **cloud dashboard on AWS**. Project idea #4 (IoT
Environmental Monitor), extended end to end.

```
 [Node A]  --LoRa data-->  [Gateway]  --WiFi/HTTP-->  [AWS EC2]  -->  browser
  ESP32 + DHT22 +          ESP32 + LoRa                Flask :8080   iot.gabr.online
  photoresistor   <--LoRa ACK--  + WiFi               (token-gated)
  (EVAL/ALARM bonus)
```

Node A reads its sensors every 3 s, runs an environment-safety rule, and
transmits over 433 MHz LoRa. The gateway ACKs every packet back over
LoRa (bidirectional link), then HTTP-POSTs the JSON over WiFi to a
token-gated Flask service on an AWS EC2 box, which serves a live
dashboard at `http://iot.gabr.online:8080/`. The LoRa link keeps working
(with USB output + ACKs) even if WiFi/internet is down.

## Repository layout

| Path | What |
|---|---|
| `firmware/common/protocol.h` | LoRa params, pin map, node ID, packet schema, `USE_LORA` switch. |
| `firmware/node_a/` | Node A — DHT22 + photoresistor → LoRa, EVAL/ALARM bonus, listens for the ACK. |
| `firmware/gateway/` | Gateway — LoRa RX → LoRa ACK → USB + WiFi HTTP POST + link-stats heartbeat. |
| `firmware/gateway/secrets.example.h` | Template for WiFi creds + cloud token. Copy to `secrets.h` (gitignored). |
| `firmware/diag_wifiscan/` | Throwaway: dumps visible 2.4 GHz WiFi (used to debug the join). |
| `dashboard/cloud_app.py` | AWS Flask service: token-gated `/ingest`, live dashboard, `/data`. |
| `dashboard/dashboard.py` | Local serial fallback dashboard, `--fake` mode for hardware-free testing. |
| `docs/report.tex` / `report.pdf` | Final report. |
| `docs/slides.md` / `slides.pptx` | Discussion PowerPoint. |
| `docs/DEMO_VIDEO.md` | Shot list for the demonstration video. |

## Hardware

2× ESP32-WROOM DevKit, 2× LoRa Ra-01 (SX1278, 433 MHz), 1× DHT22,
1× analog photoresistor module, ~17 cm wire antennas, breadboards +
jumpers, 100 nF caps. Pin map in `firmware/common/protocol.h`.

> The Ra-01 is a **3.3 V** part — power from the ESP32's 3V3 pin, never
> 5 V, 100 nF across VCC–GND. Both Ra-01s need a ~17 cm antenna wire.
> The ESP32-WROOM is **2.4 GHz only** — a 5 GHz hotspot won't be seen.

## Secrets

Real WiFi credentials and the cloud ingest token are **not** in the
repo. Before building the gateway:

```bash
cp firmware/gateway/secrets.example.h firmware/gateway/secrets.h
# then edit secrets.h with the real SSID / password / URL / token
```

`secrets.h` is gitignored. iOS hotspot SSIDs use a typographic
apostrophe `’` (U+2019), not `'` — paste the exact character or the
join silently fails.

## Building the firmware (Arduino IDE or arduino-cli)

1. Install the **ESP32 board package** (Boards Manager URL:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`).
2. Libraries: **LoRa** (Sandeep Mistry), **DHT sensor library**
   (Adafruit) + **Adafruit Unified Sensor**. WiFi/HTTPClient ship with
   the ESP32 core.
3. Flash `firmware/node_a` and `firmware/gateway` to the two boards
   (FQBN `esp32:esp32:esp32`), one sketch per board.

**Fallback:** set `#define USE_LORA 0` in a board's `protocol.h` to
stream CSV straight to USB if a radio fails on demo day.

## Cloud (already deployed)

`cloud_app.py` runs as a systemd service on an AWS EC2 box, port 8080,
in its own venv, gated by an `X-Token` header (token via systemd env
var). A Route 53 record maps `iot.gabr.online`. It is isolated by port
from the unrelated service on that box. Open
`http://iot.gabr.online:8080/` to view it.

Local alternative without the cloud:

```bash
cd dashboard && pip install -r requirements.txt
python dashboard.py --port COM5     # gateway on USB
python dashboard.py --fake          # no hardware
```

## Demo procedure (also the video script)

1. Start the AWS dashboard URL in a browser; power the **gateway**
   (it joins WiFi, prints `# WiFi OK`).
2. Power **Node A**; within ~10 s the dashboard shows it ONLINE with
   live temp/humidity/light and a day/night badge; Node A's serial
   prints `ACK ok seq=N` (the bidirectional link).
3. Cover the photoresistor → light drops, badge flips to **NIGHT**.
4. Warm the DHT22 past the overheat threshold (or cover the LDR and
   breathe on it) → the **ENVIRONMENT ALARM** banner pulses on the
   cloud dashboard; remove the hazard → it clears.
5. Show the gateway link-stats strip (packet count + RSSI).

## Deliverables checklist

- [x] Microcontroller code — `firmware/`
- [x] Cloud service + dashboard — `dashboard/cloud_app.py` (deployed)
- [x] Final report — `docs/report.pdf`
- [x] Discussion PowerPoint — `docs/slides.pptx`
- [ ] Demonstration video — record per `docs/DEMO_VIDEO.md`
- [ ] Fill in team names/IDs + contributions + real-hardware test rows
      (search the report/slides for `FILL IN`)

## Design notes

Compact CSV on the radio (no JSON lib on the node); JSON built once on
the gateway. The LoRa ACK is sent **before** the WiFi POST so the
node's round-trip stays tight even on a slow hotspot. WiFi is a
non-fatal added stage — losing it degrades to LoRa-ACK + USB, not an
outage. Every board's firmware is one explicit finite state machine
mapping 1:1 onto the diagrams in the report.
