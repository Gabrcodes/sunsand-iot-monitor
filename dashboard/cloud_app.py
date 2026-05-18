#!/usr/bin/env python3
"""
SunSand IoT monitor -- cloud dashboard (runs on the EC2 box, port 8080).

The LoRa gateway joins WiFi and HTTP-POSTs one JSON object per packet to
/ingest (token-gated). This service folds those into live state and
serves the same dashboard page the local serial version does.

    POST /ingest   header X-Token: <SECRET>   body: one JSON telemetry obj
    GET  /         the live dashboard
    GET  /data     JSON snapshot (browser polls this)

Run:  SUNSAND_TOKEN=... python3 cloud_app.py   (listens on 0.0.0.0:8080)
"""
import json
import os
import threading
import time
from collections import deque

from flask import Flask, jsonify, render_template_string, request

SECRET = os.environ.get("SUNSAND_TOKEN", "change-me")
PORT = int(os.environ.get("SUNSAND_PORT", "8080"))

LOCK = threading.Lock()
STATE = {
    "A": {"online": False, "last": None, "hist": deque(maxlen=120)},
    "GW": {"last": None},
}
OFFLINE_AFTER_S = 12


def ingest(line: str) -> None:
    line = line.strip()
    if not line or line.startswith("#"):
        return
    try:
        msg = json.loads(line)
    except ValueError:
        return
    node = msg.get("node")
    now = time.time()
    with LOCK:
        if node == "A":
            STATE["A"]["last"] = msg
            STATE["A"]["online"] = True
            STATE["A"]["last_rx"] = now
            STATE["A"]["hist"].append(
                {"t": now, "temp": msg.get("temp"), "hum": msg.get("hum")}
            )
        elif node == "GW":
            STATE["GW"]["last"] = msg
            STATE["GW"]["last_rx"] = now


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
  .wrap{display:grid;grid-template-columns:1fr;gap:18px;padding:22px;
        max-width:680px;margin:auto}
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
  .gw{max-width:680px;margin:0 auto 24px;padding:0 22px}
  .gwcard{background:var(--card);border:1px solid #243352;border-radius:14px;
          padding:16px 22px;display:flex;gap:34px;flex-wrap:wrap;
          color:var(--mut);font-size:13px}
  .gwcard b{color:var(--ink)}
</style></head><body>
<header>
  <div><h1>SunSand IoT Monitor</h1>
  <div class="sub">Node A &rarr; LoRa &rarr; gateway &rarr; WiFi &rarr; this cloud dashboard</div></div>
  <div class="sub" id="clock"></div>
</header>

<div class="wrap">
  <div class="card" id="cardA">
    <div style="display:flex;justify-content:space-between;align-items:center">
      <h2>Node A &mdash; Environment</h2><span class="pill off" id="stA">OFFLINE</span></div>
    <div class="grid">
      <div class="metric"><div class="v" id="A_t">--</div><div class="l">Temp &deg;C</div></div>
      <div class="metric"><div class="v" id="A_h">--</div><div class="l">Humidity %</div></div>
      <div class="metric"><div class="v" id="A_l">--</div><div class="l">Light</div></div>
      <div class="metric"><div class="v" id="A_n">--</div><div class="l">Day / Night</div></div>
    </div>
    <div class="alarm" id="A_alarm">&#9888; ENVIRONMENT ALARM</div>
    <div class="foot" id="A_f">no packets yet</div>
  </div>
</div>

<div class="gw"><div class="gwcard">
  <div>Gateway uptime <b id="gw_up">--</b></div>
  <div>Node A packets <b id="gw_a">--</b></div>
  <div>Link A <b id="gw_ar">--</b> dBm</div>
</div></div>

<script>
function ms(x){if(x==null)return"--";var s=Math.floor(x/1000);
  var h=Math.floor(s/3600),m=Math.floor((s%3600)/60);return h+"h "+m+"m";}
async function tick(){
  let r=await fetch('/data'); let d=await r.json();
  document.getElementById('clock').textContent=new Date().toLocaleTimeString();
  let A=d.A;
  document.getElementById('stA').className='pill '+(A.online?'on':'off');
  document.getElementById('stA').textContent=A.online?'ONLINE':'OFFLINE';
  if(A.last){let m=A.last;
    A_t.textContent=m.temp; A_h.textContent=m.hum;
    A_l.textContent=Math.round(m.light);
    A_n.textContent=m.night? 'NIGHT':'day';
    A_f.textContent='seq '+m.seq+'  ·  RSSI '+m.rssi+' dBm  ·  SNR '+m.snr+' dB';
    document.getElementById('A_alarm').className='alarm'+(m.alarm?' show':'');}
  if(d.GW.last){let g=d.GW.last;
    gw_up.textContent=ms(g.uptime_ms); gw_a.textContent=g.a_count;
    gw_ar.textContent=g.a_rssi;}
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
        s = STATE["A"]
        online = s["online"] and (now - s.get("last_rx", 0) < OFFLINE_AFTER_S)
        out = {
            "A": {"online": online, "last": s["last"]},
            "GW": {"last": STATE["GW"]["last"]},
        }
    return jsonify(out)


@app.route("/ingest", methods=["POST"])
def ingest_route():
    if request.headers.get("X-Token", "") != SECRET:
        return ("forbidden", 403)
    body = request.get_data(as_text=True) or ""
    for line in body.splitlines() or [body]:
        ingest(line)
    return ("", 204)


@app.route("/health")
def health():
    return jsonify({"ok": True})


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=PORT, threaded=True)
