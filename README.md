# SunSand IoT Monitor

Microprocessor course project — a two-node **LoRa wireless sensor system**
with a live laptop dashboard. Project idea #4 (IoT Environmental Monitor),
extended from one board to three.

```
 [Node A] Weather        \
  ESP32 + DHT22 + BH1750   \   LoRa 433 MHz
                            >-->  [Gateway]  --USB serial-->  [Laptop]
 [Node B] Env./Security    /      ESP32 + LoRa    JSON lines   dashboard
  ESP32 + DHT22 + PIR      /       (no sensors)
  + reed switch + buzzer
```

Two ESP32 sensor nodes read their sensors every 3 s, send a compact CSV
packet over a 433 MHz LoRa radio link to a third ESP32 (the gateway).
The gateway attaches link quality and prints one JSON line per packet
over USB serial. A Python/Flask dashboard on the laptop reads that
serial stream and shows the data live in a browser.

## Repository layout

| Path | What |
|---|---|
| `firmware/common/protocol.h` | Shared constants: LoRa parameters, pin map, node IDs, packet schema, `USE_LORA` transport switch. |
| `firmware/node_a/` | Node A sketch — DHT22 + BH1750 → LoRa. |
| `firmware/node_b/` | Node B sketch — DHT22 + PIR + reed + buzzer → LoRa, with the alarm state. |
| `firmware/gateway/` | Gateway sketch — LoRa RX → JSON over USB serial + link-stats heartbeat. |
| `dashboard/dashboard.py` | Laptop dashboard (Flask + pyserial). `--fake` mode for hardware-free testing. |
| `docs/report.tex` / `report.pdf` | Final report (design, implementation, testing, contributions). |
| `docs/slides.md` / `slides.pptx` | Discussion PowerPoint. |
| `docs/DEMO_VIDEO.md` | Shot list / script for recording the demonstration video. |

## Hardware

3× ESP32-WROOM DevKit, 3× LoRa Ra-01 (SX1278, 433 MHz), 2× DHT22,
1× BH1750, 1× HC-SR501 PIR, 1× reed switch + magnet, 1× active buzzer,
breadboards + jumpers, 100 nF caps. Pin assignments are in
`firmware/common/protocol.h` and in the report.

> The Ra-01 is a **3.3 V** part — power it from the ESP32's 3V3 pin,
> never 5 V. 100 nF ceramic across its VCC–GND.

## Building the firmware (Arduino IDE)

1. Install the **ESP32 board package**: Boards Manager → search "esp32"
   (Espressif Systems) → install. Select board *ESP32 Dev Module*.
2. Install these libraries (Library Manager):
   - **LoRa** by Sandeep Mistry
   - **DHT sensor library** by Adafruit (+ **Adafruit Unified Sensor**)
   - **BH1750** by Christopher Laws
3. Open `firmware/node_a/node_a.ino` (and `node_b`, `gateway`),
   select the right COM port, and Upload — one sketch per board.

Each sketch folder already contains its own copy of `protocol.h`
so the Arduino IDE finds it with no path setup.

**Fallback (no LoRa):** set `#define USE_LORA 0` in a node's
`protocol.h` and the node prints its CSV straight to USB instead of
transmitting — the dashboard reads it identically. Use this if a radio
module fails on demo day.

## Running the dashboard

```bash
cd dashboard
pip install -r requirements.txt

# real hardware: gateway plugged into COM5 (Windows) / /dev/ttyUSB0 (Linux)
python dashboard.py --port COM5

# no hardware yet — synthesise telemetry to test the dashboard
python dashboard.py --fake
```

Then open <http://127.0.0.1:8000>.

## Demo procedure (also the video script)

1. Start the dashboard. Power the **gateway** (USB) — it prints
   `gateway init OK`.
2. Power **Node B**. Within ~10 s the dashboard shows Node B ONLINE
   with live temperature/humidity.
3. Wave a hand in front of the PIR → `Motion: YES`. Move the magnet
   away from the reed switch → `Door: OPEN`. Do both at once → the
   **intrusion alarm banner** appears and the buzzer sounds.
4. Power **Node A**. Node A appears ONLINE; shine a phone torch at the
   BH1750 → the light value jumps.
5. Show the gateway link-stats strip (per-node packet counts + RSSI).

See `docs/DEMO_VIDEO.md` for the full shot list.

## Deliverables checklist

- [x] Microcontroller code — `firmware/`
- [x] Final report — `docs/report.pdf`
- [x] Discussion PowerPoint — `docs/slides.pptx`
- [ ] Demonstration video — record per `docs/DEMO_VIDEO.md`
- [ ] Fill in team names/IDs + contributions + real-hardware test results
      (search the report and slides for `FILL IN`)

## Design note

The radio payload is compact CSV (`A,12,24.5,45.0,320.0`), not JSON —
that keeps each packet tiny and means the constrained nodes never need
a JSON library. JSON is built once, on the gateway, where it is
convenient for the dashboard. Every board's firmware is a single
explicit finite state machine, so the code maps 1:1 onto the state
diagrams in the report.
