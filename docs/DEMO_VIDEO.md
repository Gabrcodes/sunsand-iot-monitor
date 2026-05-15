# Demonstration video — shot list

Target length: **2–3 minutes**. Phone camera is fine. Record in a quiet
room so the buzzer is audible. Keep the laptop dashboard on screen for
most of it.

## Before recording

- All three boards wired and flashed (node A, node B, gateway).
- Laptop running `python dashboard.py --port COM<n>` with the browser
  open at `http://127.0.0.1:8000`, full-screen.
- Have the reed-switch magnet in hand and a phone with a torch ready.

## Shots, in order

1. **Intro (10 s).** Say the project name, the team, and the one-line
   idea: "two LoRa sensor nodes and a gateway feeding a live dashboard."
2. **The hardware (15 s).** Pan slowly across the three breadboards.
   Point at each: "Node A — weather. Node B — environment and security.
   The gateway — no sensors, just the radio and USB to the laptop."
3. **Power-up (15 s).** Plug in the gateway; show its serial monitor
   printing `gateway init OK -> LISTEN`. Then power Node B; within
   ~10 s the dashboard flips Node B to **ONLINE** with live temp/humidity.
4. **Live sensors (20 s).** Breathe on / hold the Node B DHT22 — the
   temperature ticks up on screen. Power Node A; shine the torch on its
   BH1750 — the lux value jumps.
5. **The bonus alarm (25 s).** Wave a hand in front of the PIR →
   "Motion: YES". Move the magnet away from the reed switch →
   "Door: OPEN". Do both together → the **red intrusion banner pulses
   and the buzzer sounds**. Move away / replace the magnet → it clears.
6. **Link stats (10 s).** Point at the gateway strip: per-node packet
   counts and RSSI updating every 5 s — "this is how we measure packet
   loss and detect an offline node."
7. **Wrap (10 s).** One sentence: what you'd add next (return path /
   WiFi). Done.

## If LoRa fails on the day

Re-flash one node with `#define USE_LORA 0` in its `protocol.h`, plug
that node straight into the laptop USB, and run the dashboard with
`--port` on that node's COM port. Same dashboard, same readings — record
the demo with the single node. Mention in the video that the system
also supports a direct-USB fallback path (it's a feature, not an excuse).
