# Demonstration video — shot list

Target length: **2–3 minutes**. Phone camera is fine. Keep the cloud
dashboard (`http://iot.gabr.online:8080/`) on screen for most of it.

## Before recording

- Both boards wired, antennas on both Ra-01s, flashed (Node A, gateway).
- The iPhone Personal Hotspot ON (Settings → Personal Hotspot screen
  left open — iOS suspends it otherwise).
- A laptop/phone browser on `http://iot.gabr.online:8080/`, full-screen.
- A way to gently warm the DHT22 (cupped warm hand or a few breaths).

## Shots, in order

1. **Intro (10 s).** Project name, team, one-liner: "a LoRa sensor node
   that ACKs back, bridges over WiFi, and shows up on a cloud dashboard
   anywhere in the world."
2. **The hardware (15 s).** Pan the two boards: "Node A — DHT22 and a
   photoresistor. Gateway — no sensors; it ACKs over LoRa and pushes to
   the cloud over WiFi."
3. **Cloud first (10 s).** Show the public URL on screen, Node A
   OFFLINE — "this is hosted on AWS, reachable from anywhere."
4. **Power-up (20 s).** Power the gateway → its serial prints
   `# WiFi OK 172.20.10.x` and `# POST 204`. Power Node A → within
   ~10 s the **cloud dashboard flips Node A ONLINE** with live temp /
   humidity / light. Cut to Node A's serial showing
   `[node A] ACK ok seq=N <- gateway` — call out the **bidirectional
   link**: the gateway is talking back.
5. **Live sensors (15 s).** Breathe on the DHT22 — temp/humidity rise
   on the dashboard. Cover the photoresistor — light drops, badge flips
   to **NIGHT**; uncover — back to **day**.
6. **The bonus alarm (20 s).** Cup the DHT22 in a warm hand until it
   crosses the overheat threshold → the **ENVIRONMENT ALARM** banner
   pulses red on the cloud dashboard. (Or: cover the LDR + breathe on
   the DHT22 → the dark+humid condensation alarm.) Remove the hazard →
   it clears in ~1 s. Note this is a real extra state in the FSM.
7. **Link stats (10 s).** Gateway strip: packet count + RSSI updating —
   "this is how we measure packet loss and detect an offline node."
8. **Wrap (10 s).** One sentence on next steps (TLS, gateway→node
   commands on the ACK channel). Done.

## If WiFi fails on the day

The gateway treats WiFi as non-fatal: it still LoRa-ACKs Node A and
prints JSON to USB. Run the local dashboard instead:
`python dashboard/dashboard.py --port COM<gateway>` and demo on that —
mention the system degrades gracefully (a feature, not an excuse).

## If LoRa fails on the day

Re-flash with `#define USE_LORA 0` in `protocol.h`, plug the node into
USB, run `dashboard.py --port` on its COM port. Same dashboard, same
readings, direct-USB fallback path.
