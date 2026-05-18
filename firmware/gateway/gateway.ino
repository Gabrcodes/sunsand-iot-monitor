/* =====================================================================
 *  Gateway  --  LoRa receiver -> WiFi/cloud bridge + LoRa ACK
 *  ESP32 + LoRa Ra-01, no sensors.
 *
 *  Listens for Node A packets. For each packet it:
 *    1. transmits a short LoRa "ACK,<seq>" back to the node (fast, before
 *       the slow WiFi work, so the node's ACK window catches it),
 *    2. prints the JSON line to USB serial (local debugging),
 *    3. HTTP-POSTs the JSON to the cloud dashboard (token-gated).
 *  Every 5 s it also posts a "GW" heartbeat (packet count, RSSI, uptime).
 *
 *  State machine:
 *      INIT --ok--> LISTEN --(packet)--> PARSE --> EMIT --> LISTEN
 *        |             |                                      ^
 *        |             +--(every 5 s)--> STATUS --------------+
 *        +--fail--> ERROR --(retry)--> INIT
 *
 *  Libraries: "LoRa" by Sandeep Mistry. WiFi + HTTPClient ship with the
 *  ESP32 core (no extra install).
 * ===================================================================== */
#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <LoRa.h>
#include "protocol.h"

/* ---- DEPLOYMENT CONFIG --------------------------------------------- *
 * Real WiFi creds + cloud token live in secrets.h (gitignored). Copy
 * secrets.example.h -> secrets.h and fill it in before building.       */
#include "secrets.h"
#define WIFI_JOIN_MS  15000UL

enum State { S_INIT, S_LISTEN, S_PARSE, S_EMIT, S_STATUS, S_ERROR };
State state = S_INIT;

/* incoming packet scratch */
char    rxBuf[64];
int     rxRssi = 0;
float   rxSnr  = 0.0f;

/* link bookkeeping (bonus: stats for the test section) */
uint32_t aCount = 0;
int      aRssi  = 0;
uint32_t aLastMs = 0;
uint32_t lastStatusMs = 0;
bool     wifiOk = false;

static bool wifiConnect(uint32_t budgetMs) {
  if (WiFi.status() == WL_CONNECTED) { wifiOk = true; return true; }
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);                 /* clear a stuck connecting state */
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < budgetMs) delay(250);
  wifiOk = (WiFi.status() == WL_CONNECTED);
  if (wifiOk) {
    Serial.print(F("# WiFi OK ")); Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("# WiFi join FAILED (continuing LoRa-only)"));
  }
  return wifiOk;
}

static void cloudPost(const char *json) {
  if (WiFi.status() != WL_CONNECTED) { wifiOk = false; return; }
  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(3000);
  if (!http.begin(CLOUD_URL)) return;
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Token", INGEST_TOKEN);
  int code = http.POST((uint8_t *)json, strlen(json));
  Serial.print(F("# POST ")); Serial.println(code);
  http.end();
}

static void sendAck(unsigned long seq) {
  LoRa.beginPacket();
  LoRa.print("ACK,");
  LoRa.print(seq);
  LoRa.endPacket();                          /* blocking ~40-60 ms       */
  LoRa.receive();                            /* straight back to RX      */
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("# gateway booting"));
}

void loop() {
  switch (state) {

    case S_INIT: {
      LoRa.setPins(LORA_PIN_SS, LORA_PIN_RST, LORA_PIN_DIO0);
      if (LoRa.begin(LORA_FREQUENCY_HZ)) {
        LoRa.setSpreadingFactor(LORA_SPREADING);
        LoRa.setSignalBandwidth(LORA_BANDWIDTH_HZ);
        LoRa.setCodingRate4(LORA_CODING_RATE);
        LoRa.setSyncWord(LORA_SYNC_WORD);
        LoRa.setTxPower(LORA_TX_POWER_DBM);
        LoRa.receive();
        Serial.println(F("# gateway LoRa OK -> LISTEN"));
        wifiConnect(WIFI_JOIN_MS);            /* non-fatal if it fails    */
        state = S_LISTEN;
      } else {
        Serial.println(F("# gateway LoRa init FAIL -> ERROR"));
        state = S_ERROR;
      }
      break;
    }

    case S_LISTEN: {
      int packetSize = LoRa.parsePacket();
      if (packetSize > 0) {
        int i = 0;
        while (LoRa.available() && i < (int)sizeof(rxBuf) - 1)
          rxBuf[i++] = (char)LoRa.read();
        rxBuf[i] = '\0';
        rxRssi = LoRa.packetRssi();
        rxSnr  = LoRa.packetSnr();
        state  = S_PARSE;
      } else if (millis() - lastStatusMs >= 5000UL) {
        state = S_STATUS;
      }
      break;
    }

    case S_PARSE: {
      if (rxBuf[0] == NODE_A_ID) {
        aCount++; aRssi = rxRssi; aLastMs = millis();
        state = S_EMIT;
      } else {
        Serial.print(F("# dropped unknown packet: "));
        Serial.println(rxBuf);
        LoRa.receive();
        state = S_LISTEN;
      }
      break;
    }

    /* ---- EMIT: ACK first (fast), then serial + cloud -------------- *
     * Node A CSV: A,<seq>,<tempC>,<humPct>,<light>,<night>,<alarm>     */
    case S_EMIT: {
      char tmp[64];
      strncpy(tmp, rxBuf, sizeof(tmp));
      tmp[sizeof(tmp) - 1] = '\0';

      strtok(tmp, ",");                       /* skip the id field      */
      char *s_seq = strtok(NULL, ",");
      unsigned long seq = s_seq ? strtoul(s_seq, NULL, 10) : 0;
      char *s_t = strtok(NULL, ",");
      char *s_h = strtok(NULL, ",");
      char *s_l = strtok(NULL, ",");
      char *s_n = strtok(NULL, ",");
      char *s_a = strtok(NULL, ",");

      sendAck(seq);                           /* 1. ACK the node first   */

      char out[192];
      snprintf(out, sizeof(out),
        "{\"node\":\"A\",\"seq\":%lu,\"temp\":%s,\"hum\":%s,"
        "\"light\":%s,\"night\":%s,\"alarm\":%s,"
        "\"rssi\":%d,\"snr\":%.1f,\"t_ms\":%lu}",
        seq, s_t ? s_t : "0", s_h ? s_h : "0", s_l ? s_l : "0",
        s_n ? s_n : "0", s_a ? s_a : "0",
        rxRssi, rxSnr, (unsigned long)millis());

      Serial.println(out);                    /* 2. local debug          */
      cloudPost(out);                         /* 3. cloud               */

      LoRa.receive();
      state = S_LISTEN;
      break;
    }

    case S_STATUS: {
      if (WiFi.status() != WL_CONNECTED) wifiConnect(4000);
      char out[200];
      snprintf(out, sizeof(out),
        "{\"node\":\"GW\",\"a_count\":%lu,\"a_rssi\":%d,"
        "\"a_age_ms\":%lu,\"uptime_ms\":%lu}",
        (unsigned long)aCount, aRssi,
        (unsigned long)(aLastMs ? millis() - aLastMs : 0),
        (unsigned long)millis());
      Serial.println(out);
      cloudPost(out);
      lastStatusMs = millis();
      LoRa.receive();
      state = S_LISTEN;
      break;
    }

    case S_ERROR: {
      delay(2000);
      Serial.println(F("# gateway retrying -> INIT"));
      state = S_INIT;
      break;
    }
  }
}
