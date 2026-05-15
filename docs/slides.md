% SunSand IoT Monitor
% [FILL IN: team member names + IDs]
% Microprocessor Course — Project Discussion · 2026-05-17

# Project concept

- Project idea **#4 — IoT Environmental Monitor**, extended.
- Instead of one board: **two LoRa sensor nodes + a gateway** (three ESP32s).
- Nodes read sensors → transmit over **433 MHz LoRa** → gateway → **USB serial** → live laptop dashboard.
- Demonstrates: I²C, 1-wire digital, GPIO, SPI+RF, UART-to-host, and an explicit finite-state-machine firmware on every board.

# Individual contributions

| Member | Contribution |
|---|---|
| [FILL IN: name 1] | [e.g. Node A firmware + sensor wiring + testing] |
| [FILL IN: name 2] | [e.g. Node B firmware + alarm logic + buzzer] |
| [FILL IN: name 3] | [e.g. Gateway firmware + LoRa bring-up] |
| [FILL IN: name 4] | [e.g. Dashboard + report + presentation] |

*(Replace every [FILL IN] before presenting.)*

# System block diagram

```
 [Node A]  Weather       \
  DHT22 + BH1750           \  LoRa 433 MHz
                            >--->  [Gateway]  --USB serial-->  [Laptop]
 [Node B]  Env./Security   /       ESP32+LoRa     JSON lines    dashboard
  DHT22 + PIR + reed      /        (no sensors)
  + buzzer (alarm)
```

- Nodes are transmit-only; gateway is RX on radio, TX on USB.
- No WiFi / broker in the room → rock-solid demo.
- *(Vector version of every diagram is in the report PDF.)*

# Sensors used

| Component | Interface | Role |
|---|---|---|
| ESP32-WROOM ×3 | — | MCU for each node + gateway |
| LoRa Ra-01 (SX1278) ×3 | SPI | 433 MHz radio |
| DHT22 ×2 | 1-wire digital | Temp + humidity |
| BH1750 ×1 | I²C | Light (lux), Node A |
| HC-SR501 PIR ×1 | GPIO | Motion, Node B |
| Reed switch ×1 | GPIO | Door open/closed, Node B |
| Active buzzer ×1 | GPIO | Alarm, Node B (bonus) |

# Circuit schematic (Node B shown)

```
            +-----------+ GPIO4   DHT22
            |           |-------- (temp/hum)
   Ra-01 ---|  ESP32    | GPIO34  HC-SR501 PIR
   (SPI)    |  Node B   | GPIO32  Reed switch (INPUT_PULLUP -> GND)
            |           | GPIO25  Buzzer
            +-----------+
```

- Ra-01 powered at **3.3 V** (never 5 V) + 100 nF decoupling.
- LoRa SPI bus: SCK 18 / MISO 19 / MOSI 23, NSS 5 / RST 27 / DIO0 26.
- Node A swaps PIR/reed/buzzer for a BH1750 on I²C (SDA 21, SCL 22).

# State machine — the firmware architecture

Every board = one `enum` of states + a `switch` in `loop()`. One `case` = one state. Direct 1:1 mapping to the diagram.

```
 Node:    INIT -> READ -> BUILD -> SEND -> WAIT -.
            \      \_ read fail _.                 \_ period _.
             \_ init fail _.      v                            v
                            ERROR <----------------------------'
                              \_ retry _-> INIT
```

# State machine — Node B (the bonus branch)

```
 INIT -> READ -> EVAL --(motion & door open)--> ALARM --.
                   \                                      \  buzzer on
                    \--(safe)----------------> BUILD <-----'  alarm=1
                                                 |
                                       SEND <----'--> WAIT --> READ
```

- **ALARM is a real extra state**, not just an `if`.
- In ALARM the node re-evaluates every 0.5 s (vs 3 s) so the buzzer clears fast.

# Key code — the state machine skeleton

```c
enum State { S_INIT, S_READ, S_BUILD, S_SEND, S_WAIT, S_ERROR };
State state = S_INIT;
void loop() {
  switch (state) {
    case S_INIT:  /* bring up sensors + radio */        break;
    case S_READ:  /* sample; bad read -> S_ERROR */     break;
    case S_BUILD: /* snprintf the CSV payload */        break;
    case S_SEND:  /* LoRa transmit (or USB fallback) */ break;
    case S_WAIT:  /* hold for the sense period */       break;
    case S_ERROR: /* delay, retry -> S_INIT */          break;
  }
}
```

# Key code — the bonus security rule

```c
case S_EVAL:
  if (pir == 1 && doorOpen == 1) state = S_ALARM;     // intrusion
  else { alarm = 0; digitalWrite(BUZZER,LOW); state = S_BUILD; }
  break;
case S_ALARM:
  alarm = 1; digitalWrite(BUZZER, HIGH);              // sound it
  state = S_BUILD;                                    // still report
  break;
```

# Code structure

- `firmware/common/protocol.h` — LoRa params, pin map, node IDs, packet schema, `USE_LORA` switch.
- `firmware/node_a` / `node_b` / `gateway` — the three sketches (one state machine each).
- `dashboard/dashboard.py` — Flask + pyserial live web dashboard, `--fake` mode for hardware-free testing.

**Design decision:** compact **CSV on the radio** (tiny packet, no JSON lib on the nodes) → JSON built once **on the gateway** for the dashboard.

# Additional features (bonus marks)

1. **Intrusion alarm state** — motion + door-open → buzzer + flagged packet.
2. **Gateway link-stats heartbeat** — per-node packet counts + RSSI every 5 s → offline detection + packet-loss measurement.
3. **Hardware-independent demo path** — `USE_LORA 0` sends straight to USB if a radio fails on the day; same dashboard.
4. **Self-healing firmware** — any fault routes to ERROR and auto-retries instead of freezing.
5. **Live styled web dashboard** — online/offline per node, animated alarm banner, projector-ready.

# Testing

- Dashboard verified hardware-free via `--fake`: page renders, `/data` valid, counters advance, alarm banner toggles. **Pass.**
- Each board prints a boot banner; INIT reports per-subsystem OK; faults auto-retry.
- Packet `seq` counter + gateway counts → packet-loss visible.
- [FILL IN: real-hardware sensor sanity + measured RSSI at demo distance]

# Demo video

- [EMBED OR LINK the recorded demo video here.]
- Shows: power on gateway → dashboard "online", power node B → live temp/hum, wave hand → motion, open "door" magnet → alarm banner + buzzer, power node A → light reading reacts to a torch.

# Conclusion

- Met brief (idea #4), extended to a real wireless sensor network.
- Clean FSM firmware, documented protocol, live dashboard, 5 bonus features.
- Future: return path (gateway→node), swap USB for WiFi/MQTT, more nodes (protocol already namespaces by node ID).

**Thank you — questions?**
