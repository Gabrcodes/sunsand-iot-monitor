#!/usr/bin/env python3
"""
SunSand IoT monitor -- laptop dashboard.

Reads the gateway's USB-serial output (one JSON object per line) and
serves a live web dashboard. Each line is one of:

    {"node":"A","seq":..,"temp":..,"hum":..,"lux":..,"rssi":..,...}
    {"node":"B","seq":..,"temp":..,"hum":..,"pir":..,"door":..,"alarm":..,...}
    {"node":"GW","a_count":..,"b_count":..,"a_rssi":..,"a_age_ms":..,...}

Usage
-----
    # real hardware: gateway plugged into COM5 (Windows) or /dev/ttyUSB0
    python dashboard.py --port COM5

    # no hardware yet: synthesise packets so the dashboard can be tested
    python dashboard.py --fake

Then open http://127.0.0.1:8000 in a browser.

Dependencies:  pip install flask pyserial   (see requirements.txt)
"""
import argparse
import json
import threading
import time
import random
from collections import deque

from flask import Flask, jsonify, render_template_string

# --------------------------------------------------------------------------
# Shared, thread-safe state. The reader thread writes it; Flask reads it.
# --------------------------------------------------------------------------
LOCK = threading.Lock()
STATE = {
    "A": {"online": False, "last": None, "hist": deque(maxlen=120)},
    "B": {"online": False, "last": None, "hist": deque(maxlen=120)},
    "GW": {"last": None},
}
OFFLINE_AFTER_S = 12  # ~4 missed 3-second packets => treat node as offline


def ingest(line: str) -> None:
    """Parse one serial line and fold it into STATE."""
    line = line.strip()
    if not line or line.startswith("#"):
        return  # gateway debug/comment lines
    try:
        msg = json.loads(line)
    except ValueError:
        return  # not a JSON telemetry line; ignore
    node = msg.get("node")
    now = time.time()
    with LOCK:
        if node in ("A", "B"):
            STATE[node]["last"] = msg
            STATE[node]["online"] = True
            STATE[node]["last_rx"] = now
            STATE[node]["hist"].append(
                {"t": now, "temp": msg.get("temp"), "hum": msg.get("hum")}
            )
        elif node == "GW":
            STATE["GW"]["last"] = msg
            STATE["GW"]["last_rx"] = now


# --------------------------------------------------------------------------
# Serial reader (real hardware)
# --------------------------------------------------------------------------
def serial_reader(port: str, baud: int) -> None:
    import serial  # pyserial; imported here so --fake needs no pyserial

    while True:
        try:
            with serial.Serial(port, baud, timeout=2) as ser:
                print(f"[dashboard] reading {port} @ {baud}")
                while True:
                    raw = ser.readline().decode("utf-8", "replace")
                    if raw:
                        ingest(raw)
        except Exception as exc:  # port unplugged, wrong COM, etc.
            print(f"[dashboard] serial error: {exc}; retrying in 3 s")
            time.sleep(3)


# --------------------------------------------------------------------------
# Fake reader (no hardware -- for testing the dashboard + the demo video)
# --------------------------------------------------------------------------
def fake_reader() -> None:
    print("[dashboard] FAKE mode: synthesising telemetry")
    a_seq = b_seq = 0
    t = 24.0
    while True:
        t += random.uniform(-0.3, 0.3)
        ingest(json.dumps({
            "node": "A", "seq": a_seq, "temp": round(t, 1),
            "hum": round(45 + random.uniform(-3, 3), 1),
            "lux": round(300 + random.uniform(-80, 400), 1),
            "rssi": random.randint(-80, -55),
            "snr": round(random.uniform(7, 11), 1),
            "t_ms": int(time.time() * 1000) % 10_000_000,
        }))
        a_seq += 1
        motion = 1 if random.random() < 0.25 else 0
        door = 1 if random.random() < 0.15 else 0
        ingest(json.dumps({
            "node": "B", "seq": b_seq,
            "temp": round(t + random.uniform(-1, 1), 1),
            "hum": round(47 + random.uniform(-3, 3), 1),
            "pir": motion, "door": door,
            "alarm": 1 if (motion and door) else 0,
            "rssi": random.randint(-82, -58),
            "snr": round(random.uniform(6, 10), 1),
            "t_ms": int(time.time() * 1000) % 10_000_000,
        }))
        b_seq += 1
        ingest(json.dumps({
            "node": "GW", "a_count": a_seq, "b_count": b_seq,
            "a_rssi": random.randint(-80, -55),
            "b_rssi": random.randint(-82, -58),
            "a_age_ms": 0, "b_age_ms": 0,
            "uptime_ms": int(time.time() * 1000) % 100_000_000,
        }))
        time.sleep(3)


# --------------------------------------------------------------------------
# Web app
# --------------------------------------------------------------------------
app = Flask(__name__)

PAGE = r"""
<!doctype html><html><head><meta charset="utf-8">
<title>SunSand IoT Monitor</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  :root{--bg:#0e1726;--card:#16243a;--ink:#e8eef7;--mut:#8aa0bd;
        --ok:#34d399;--warn:#fbbf24;--bad:#f87171;--acc:#60a5fa}
  *{box-sizing:border-box;font-family:Segoe UI,system-ui,sans-serif}
  body{margin:0;background:var(--bg);color:var(--ink)}
  header{padding:18px 26px;border-bottom:1px solid #243352;
         display:flex;justify-content:space-between;align-items:center}
  h1{font-size:20px;margin:0;font-weight:600}
  .sub{color:var(--mut);font-size:13px}
  .wrap{display:grid;grid-template-columns:1fr 1fr;gap:18px;padding:22px;
        max-width:1100px;margin:auto}
  .card{background:var(--card);border-radius:14px;padding:20px 22px;
        border:1px solid #243352}
  .card h2{margin:0 0 4px;font-size:16px}
  .pill{font-size:12px;padding:3px 10px;border-radius:999px;font-weight:600}
  .on{background:rgba(52,211,153,.15);color:var(--ok)}
  .off{background:rgba(248,113,113,.15);color:var(--bad)}
  .grid{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-top:16px}
  .metric{background:#0f1b2e;border-radius:10px;padding:14px}
  .metric .v{font-size:30px;font-weight:700}
  .metric .l{color:var(--mut);font-size:12px;text-transform:uppercase;
             letter-spacing:.06em}
  .alarm{background:rgba(248,113,113,.18);color:var(--bad);font-weight:700;
         padding:12px 16px;border-radius:10px;margin-top:14px;display:none}
  .alarm.show{display:block;animation:pulse 1s infinite}
  @keyframes pulse{50%{opacity:.55}}
  .foot{color:var(--mut);font-size:12px;margin-top:14px}
  .gw{max-width:1100px;margin:0 auto 24px;padding:0 22px}
  .gwcard{background:var(--card);border:1px solid #243352;border-radius:14px;
          padding:16px 22px;display:flex;gap:34px;flex-wrap:wrap;
          color:var(--mut);font-size:13px}
  .gwcard b{color:var(--ink)}
</style></head><body>
<header>
  <div><h1>SunSand IoT Monitor</h1>
  <div class="sub">2 LoRa sensor nodes &rarr; gateway &rarr; this dashboard</div></div>
  <div class="sub" id="clock"></div>
</header>

<div class="wrap">
  <div class="card" id="cardA">
    <div style="display:flex;justify-content:space-between;align-items:center">
      <h2>Node A &mdash; Weather</h2><span class="pill off" id="stA">OFFLINE</span></div>
    <div class="grid">
      <div class="metric"><div class="v" id="A_t">--</div><div class="l">Temp &deg;C</div></div>
      <div class="metric"><div class="v" id="A_h">--</div><div class="l">Humidity %</div></div>
      <div class="metric"><div class="v" id="A_l">--</div><div class="l">Light lux</div></div>
      <div class="metric"><div class="v" id="A_r">--</div><div class="l">RSSI dBm</div></div>
    </div>
    <div class="foot" id="A_f">no packets yet</div>
  </div>

  <div class="card" id="cardB">
    <div style="display:flex;justify-content:space-between;align-items:center">
      <h2>Node B &mdash; Environment / Security</h2><span class="pill off" id="stB">OFFLINE</span></div>
    <div class="grid">
      <div class="metric"><div class="v" id="B_t">--</div><div class="l">Temp &deg;C</div></div>
      <div class="metric"><div class="v" id="B_h">--</div><div class="l">Humidity %</div></div>
      <div class="metric"><div class="v" id="B_p">--</div><div class="l">Motion</div></div>
      <div class="metric"><div class="v" id="B_d">--</div><div class="l">Door</div></div>
    </div>
    <div class="alarm" id="B_alarm">&#9888; INTRUSION ALARM &mdash; motion while door open</div>
    <div class="foot" id="B_f">no packets yet</div>
  </div>
</div>

<div class="gw"><div class="gwcard">
  <div>Gateway uptime <b id="gw_up">--</b></div>
  <div>Node A packets <b id="gw_a">--</b></div>
  <div>Node B packets <b id="gw_b">--</b></div>
  <div>Link A <b id="gw_ar">--</b> dBm</div>
  <div>Link B <b id="gw_br">--</b> dBm</div>
</div></div>

<script>
function ms(x){if(x==null)return"--";var s=Math.floor(x/1000);
  var h=Math.floor(s/3600),m=Math.floor((s%3600)/60);return h+"h "+m+"m";}
async function tick(){
  let r=await fetch('/data'); let d=await r.json();
  document.getElementById('clock').textContent=new Date().toLocaleTimeString();
  // Node A
  let A=d.A;
  document.getElementById('stA').className='pill '+(A.online?'on':'off');
  document.getElementById('stA').textContent=A.online?'ONLINE':'OFFLINE';
  if(A.last){let m=A.last;
    A_t.textContent=m.temp; A_h.textContent=m.hum;
    A_l.textContent=Math.round(m.lux); A_r.textContent=m.rssi;
    A_f.textContent='seq '+m.seq+'  ·  SNR '+m.snr+' dB';}
  // Node B
  let B=d.B;
  document.getElementById('stB').className='pill '+(B.online?'on':'off');
  document.getElementById('stB').textContent=B.online?'ONLINE':'OFFLINE';
  if(B.last){let m=B.last;
    B_t.textContent=m.temp; B_h.textContent=m.hum;
    B_p.textContent=m.pir? 'YES':'no';
    B_d.textContent=m.door? 'OPEN':'shut';
    B_f.textContent='seq '+m.seq+'  ·  RSSI '+m.rssi+' dBm';
    document.getElementById('B_alarm').className='alarm'+(m.alarm?' show':'');}
  // Gateway
  if(d.GW.last){let g=d.GW.last;
    gw_up.textContent=ms(g.uptime_ms); gw_a.textContent=g.a_count;
    gw_b.textContent=g.b_count; gw_ar.textContent=g.a_rssi;
    gw_br.textContent=g.b_rssi;}
}
tick(); setInterval(tick,1000);
</script>
</body></html>
"""


@app.route("/")
def index():
    return render_template_string(PAGE)


@app.route("/data")
def data():
    now = time.time()
    with LOCK:
        out = {"A": {}, "B": {}, "GW": {}}
        for n in ("A", "B"):
            s = STATE[n]
            online = s["online"] and (now - s.get("last_rx", 0) < OFFLINE_AFTER_S)
            out[n] = {"online": online, "last": s["last"]}
        out["GW"] = {"last": STATE["GW"]["last"]}
    return jsonify(out)


def main() -> None:
    ap = argparse.ArgumentParser(description="SunSand IoT dashboard")
    ap.add_argument("--port", help="serial port of the gateway (e.g. COM5)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--fake", action="store_true",
                    help="synthesise telemetry instead of reading serial")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--web-port", type=int, default=8000)
    args = ap.parse_args()

    if args.fake:
        threading.Thread(target=fake_reader, daemon=True).start()
    elif args.port:
        threading.Thread(target=serial_reader,
                         args=(args.port, args.baud), daemon=True).start()
    else:
        ap.error("give --port COMx for hardware, or --fake to test")

    print(f"[dashboard] open http://{args.host}:{args.web_port}")
    app.run(host=args.host, port=args.web_port, threaded=True)


if __name__ == "__main__":
    main()
