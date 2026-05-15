/* =====================================================================
 *  Gateway  --  LoRa receiver / USB-serial bridge
 *  ESP32 + LoRa Ra-01, no sensors.
 *
 *  Listens continuously for packets from node A and node B, attaches the
 *  received signal strength (RSSI) and SNR, and prints one clean JSON
 *  line per packet over USB serial to the laptop dashboard. Every 5 s it
 *  also emits a "GW" heartbeat with per-node packet counts and last-seen
 *  times -- this is what the dashboard uses to show a node as offline,
 *  and it doubles as the packet-loss measurement for the test section.
 *
 *  State machine:
 *
 *      INIT --ok--> LISTEN --(packet)--> PARSE --> EMIT --> LISTEN
 *        |             |                                      ^
 *        |             +--(every 5 s)--> STATUS --------------+
 *        +--fail--> ERROR --(retry)--> INIT
 *
 *  Library: "LoRa" by Sandeep Mistry.
 * ===================================================================== */
#include <SPI.h>
#include <LoRa.h>
#include "protocol.h"

enum State { S_INIT, S_LISTEN, S_PARSE, S_EMIT, S_STATUS, S_ERROR };
State state = S_INIT;

/* incoming packet scratch */
char    rxBuf[64];
int     rxRssi = 0;
float   rxSnr  = 0.0f;

/* per-node bookkeeping (bonus: link stats for the test section) */
uint32_t aCount = 0, bCount = 0;
int      aRssi  = 0, bRssi  = 0;
uint32_t aLastMs = 0, bLastMs = 0;
uint32_t lastStatusMs = 0;

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
        LoRa.receive();                    /* enter continuous RX        */
        Serial.println(F("# gateway init OK -> LISTEN"));
        state = S_LISTEN;
      } else {
        Serial.println(F("# gateway LoRa init FAIL -> ERROR"));
        state = S_ERROR;
      }
      break;
    }

    /* ---- LISTEN: poll for a packet, and emit periodic status ------ */
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

    /* ---- PARSE: split the CSV and update per-node stats ----------- */
    case S_PARSE: {
      /* first character is the node id */
      char id = rxBuf[0];
      if (id == NODE_A_ID) {
        aCount++; aRssi = rxRssi; aLastMs = millis();
      } else if (id == NODE_B_ID) {
        bCount++; bRssi = rxRssi; bLastMs = millis();
      } else {
        Serial.print(F("# dropped unknown packet: "));
        Serial.println(rxBuf);
        LoRa.receive();
        state = S_LISTEN;
        break;
      }
      state = S_EMIT;
      break;
    }

    /* ---- EMIT: rebuild as a JSON line for the dashboard ----------- */
    case S_EMIT: {
      char out[160];
      char id = rxBuf[0];

      /* strtok over a mutable copy of the payload */
      char tmp[64];
      strncpy(tmp, rxBuf, sizeof(tmp));
      tmp[sizeof(tmp) - 1] = '\0';

      strtok(tmp, ",");                       /* skip the id field      */
      char *s_seq = strtok(NULL, ",");
      unsigned long seq = s_seq ? strtoul(s_seq, NULL, 10) : 0;

      if (id == NODE_A_ID) {
        char *s_t = strtok(NULL, ",");
        char *s_h = strtok(NULL, ",");
        char *s_l = strtok(NULL, ",");
        snprintf(out, sizeof(out),
          "{\"node\":\"A\",\"seq\":%lu,\"temp\":%s,\"hum\":%s,"
          "\"lux\":%s,\"rssi\":%d,\"snr\":%.1f,\"t_ms\":%lu}",
          seq, s_t ? s_t : "0", s_h ? s_h : "0", s_l ? s_l : "0",
          rxRssi, rxSnr, (unsigned long)millis());
      } else { /* NODE_B_ID */
        char *s_t = strtok(NULL, ",");
        char *s_h = strtok(NULL, ",");
        char *s_p = strtok(NULL, ",");
        char *s_d = strtok(NULL, ",");
        char *s_a = strtok(NULL, ",");
        snprintf(out, sizeof(out),
          "{\"node\":\"B\",\"seq\":%lu,\"temp\":%s,\"hum\":%s,"
          "\"pir\":%s,\"door\":%s,\"alarm\":%s,"
          "\"rssi\":%d,\"snr\":%.1f,\"t_ms\":%lu}",
          seq, s_t ? s_t : "0", s_h ? s_h : "0",
          s_p ? s_p : "0", s_d ? s_d : "0", s_a ? s_a : "0",
          rxRssi, rxSnr, (unsigned long)millis());
      }

      Serial.println(out);                    /* <-- the dashboard line */
      LoRa.receive();                         /* back to RX             */
      state = S_LISTEN;
      break;
    }

    /* ---- STATUS: 5 s heartbeat with link stats (bonus) ------------ */
    case S_STATUS: {
      char out[200];
      snprintf(out, sizeof(out),
        "{\"node\":\"GW\",\"a_count\":%lu,\"b_count\":%lu,"
        "\"a_rssi\":%d,\"b_rssi\":%d,"
        "\"a_age_ms\":%lu,\"b_age_ms\":%lu,\"uptime_ms\":%lu}",
        (unsigned long)aCount, (unsigned long)bCount, aRssi, bRssi,
        (unsigned long)(aLastMs ? millis() - aLastMs : 0),
        (unsigned long)(bLastMs ? millis() - bLastMs : 0),
        (unsigned long)millis());
      Serial.println(out);
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
