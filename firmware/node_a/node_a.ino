/* =====================================================================
 *  Node A  --  environmental safety node (the only deployed node)
 *  ESP32 + LoRa Ra-01 (SX1278) + DHT22 + photoresistor module
 *
 *  Reads temperature and humidity (DHT22) and ambient light (analog
 *  photoresistor module) every SENSE_PERIOD_MS, evaluates a small
 *  environment-safety rule, packs the result into a short CSV line, and
 *  transmits it over LoRa to the gateway.
 *
 *  BONUS state machine -- the EVAL/ALARM branch. ALARM is raised when:
 *    (a) the enclosure is too hot  (tempC >= TEMP_HOT_C)        OR
 *    (b) it is dark AND humidity is high (humPct >= HUM_HIGH_PCT)
 *  Both map to real SunSand Net risks: thermal stress on the LiFePO4
 *  battery / electronics, and overnight condensation on the electronics
 *  in an unheated rural enclosure. The photoresistor is what
 *  distinguishes "day" from "night" in rule (b).
 *
 *      INIT --ok--> READ --> EVAL --+--(safe)----> BUILD --> SEND --> WAIT
 *        |                          |                                  |
 *        |                          +--(hot | dark&humid)--> ALARM ----+
 *        |                                (alarm = 1)                   |
 *        +--fail--> ERROR --(retry)--> INIT     WAIT --(period)--> READ
 *
 *  Libraries (Arduino IDE Library Manager):
 *    - "LoRa"               by Sandeep Mistry
 *    - "DHT sensor library" by Adafruit (+ "Adafruit Unified Sensor")
 *
 *  The photoresistor is a plain analog module (AO pin) -- no library,
 *  no I2C, one ADC pin. Reading is a raw, uncalibrated 0..4095 light
 *  index; "dark" is decided by a bench-tuned threshold.
 * ===================================================================== */
#include "protocol.h"        /* defines USE_LORA -- include first */
#include <DHT.h>
#if USE_LORA
#include <SPI.h>
#include <LoRa.h>            /* only needed when actually using the radio */
#endif

/* ---- Sensor wiring (kept off the Ra-01 SPI/control + strap pins) --- */
#define DHT_PIN    4             /* DHT22 data                           */
#define DHT_TYPE   DHT22
#define LIGHT_PIN  34            /* photoresistor module AO (ADC1, in)   */

/* ---- Bonus-rule thresholds (watch the serial log and tune) --------- *
 * Each raw reading is printed every cycle so you can pick these on the
 * bench instead of guessing. With the common LM393/KY-018 photoresistor
 * wiring the ADC value RISES as it gets DARKER; flip LIGHT_DARK_INVERT
 * if your module reads the other way.                                   */
#define TEMP_HOT_C       38.0f   /* enclosure-overheat threshold (degC)  */
#define HUM_HIGH_PCT     80.0f   /* condensation-risk humidity (%)       */
#define LIGHT_DARK_ADC   2400    /* 12-bit ADC 0..4095: "dark" if >=     */
#define LIGHT_DARK_INVERT 0
#define ACK_WINDOW_MS     600    /* how long to listen for the GW ACK     */

DHT dht(DHT_PIN, DHT_TYPE);

/* ---- State machine ------------------------------------------------- */
enum State { S_INIT, S_READ, S_EVAL, S_ALARM, S_BUILD, S_SEND,
             S_WAITACK, S_WAIT, S_ERROR };
State    state       = S_INIT;
uint32_t seq         = 0;        /* packet counter (lets the gateway     */
                                 /* measure packet loss in testing)      */
uint32_t sentSeq     = 0;        /* the seq we just transmitted          */
int      ackOk       = 0;        /* 1 if the gateway ACKed that packet   */
uint32_t ackDeadline = 0;
uint32_t lastSenseMs = 0;
char     packet[48];

/* live readings */
float tempC = NAN, humPct = NAN;
int   lightRaw = 0, night = 0, alarmOn = 0;  /* 'alarm' is a libc fn */

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("[node A] booting (environmental safety node)"));
}

void loop() {
  switch (state) {

    /* ---- INIT: bring up sensor + radio ---------------------------- */
    case S_INIT: {
      dht.begin();
      analogReadResolution(12);            /* photoresistor on ADC1     */

#if USE_LORA
      LoRa.setPins(LORA_PIN_SS, LORA_PIN_RST, LORA_PIN_DIO0);
      bool okRadio = LoRa.begin(LORA_FREQUENCY_HZ);
      if (okRadio) {
        LoRa.setSpreadingFactor(LORA_SPREADING);
        LoRa.setSignalBandwidth(LORA_BANDWIDTH_HZ);
        LoRa.setCodingRate4(LORA_CODING_RATE);
        LoRa.setSyncWord(LORA_SYNC_WORD);
        LoRa.setTxPower(LORA_TX_POWER_DBM);
        Serial.println(F("[node A] init OK -> READ"));
        state = S_READ;
      } else {
        Serial.println(F("[node A] LoRa init FAIL -> ERROR"));
        state = S_ERROR;
      }
#else
      /* radio-free bench mode: skip the Ra-01 entirely and stream the
       * CSV + debug lines straight to USB serial.                      */
      Serial.println(F("[node A] USE_LORA=0: serial-only mode -> READ"));
      state = S_READ;
#endif
      break;
    }

    /* ---- READ: sample the sensors --------------------------------- */
    case S_READ: {
      tempC    = dht.readTemperature();    /* DHT22: 1 read / 2 s max   */
      humPct   = dht.readHumidity();
      lightRaw = analogRead(LIGHT_PIN);    /* raw, uncalibrated 0..4095 */
      bool darkByAdc = (lightRaw >= LIGHT_DARK_ADC);
      night = (LIGHT_DARK_INVERT ? !darkByAdc : darkByAdc) ? 1 : 0;

      Serial.print(F("[node A] temp=")); Serial.print(tempC);
      Serial.print(F(" hum="));   Serial.print(humPct);
      Serial.print(F(" light=")); Serial.print(lightRaw);
      Serial.print(F(" night=")); Serial.println(night);

      if (isnan(tempC) || isnan(humPct)) {
        Serial.println(F("[node A] DHT read failed -> ERROR"));
        state = S_ERROR;
      } else {
        state = S_EVAL;
      }
      break;
    }

    /* ---- EVAL: the environment-safety rule (bonus) --------------- */
    case S_EVAL: {
      bool hot          = (tempC  >= TEMP_HOT_C);
      bool condensation = (night == 1 && humPct >= HUM_HIGH_PCT);
      if (hot || condensation) {
        state = S_ALARM;
      } else {
        alarmOn = 0;
        state = S_BUILD;
      }
      break;
    }

    /* ---- ALARM: enclosure overheat, or overnight condensation ---- */
    case S_ALARM: {
      alarmOn = 1;
      Serial.println((tempC >= TEMP_HOT_C)
        ? F("[node A] !! ALARM: enclosure overheating !!")
        : F("[node A] !! ALARM: overnight condensation risk !!"));
      state = S_BUILD;                      /* still report the packet  */
      break;
    }

    /* ---- BUILD: format the CSV payload ---------------------------- *
     * Schema:  A,<seq>,<tempC>,<humPct>,<light>,<night 0|1>,<alarm 0|1> */
    case S_BUILD: {
      snprintf(packet, sizeof(packet), "%c,%lu,%.1f,%.1f,%d,%d,%d",
               NODE_A_ID, (unsigned long)seq, tempC, humPct,
               lightRaw, night, alarmOn);
      state = S_SEND;
      break;
    }

    /* ---- SEND: transmit over LoRa (or USB in fallback mode) -------- */
    case S_SEND: {
#if USE_LORA
      LoRa.beginPacket();
      LoRa.print(packet);
      LoRa.endPacket();                    /* blocking; ~50-80 ms       */
#else
      Serial.println(packet);              /* fallback: straight to USB */
#endif
      Serial.print(F("[node A] sent: ")); Serial.println(packet);
      sentSeq = seq;
      seq++;
      lastSenseMs = millis();
#if USE_LORA
      ackOk = 0;
      LoRa.receive();                        /* flip to RX for the ACK   */
      ackDeadline = millis() + ACK_WINDOW_MS;
      state = S_WAITACK;
#else
      state = S_WAIT;                        /* no radio, no ACK         */
#endif
      break;
    }

#if USE_LORA
    /* ---- WAITACK: listen briefly for the gateway's ACK ------------ */
    case S_WAITACK: {
      int sz = LoRa.parsePacket();
      if (sz > 0) {
        char rx[24];
        int i = 0;
        while (LoRa.available() && i < (int)sizeof(rx) - 1)
          rx[i++] = (char)LoRa.read();
        rx[i] = '\0';
        if (strncmp(rx, "ACK,", 4) == 0 &&
            strtoul(rx + 4, NULL, 10) == sentSeq) {
          ackOk = 1;
          Serial.print(F("[node A] ACK ok  seq=")); Serial.print(sentSeq);
          Serial.print(F("  (rssi ")); Serial.print(LoRa.packetRssi());
          Serial.println(F(" dBm) <- gateway"));
        }
      }
      if (ackOk || millis() >= ackDeadline) {
        if (!ackOk) {
          Serial.print(F("[node A] no ACK for seq="));
          Serial.println(sentSeq);
        }
        state = S_WAIT;
      }
      break;
    }
#endif

    /* ---- WAIT: hold until the next sense period ------------------- */
    case S_WAIT: {
      /* In alarm, re-evaluate fast so the dashboard banner clears
       * quickly once the hazard is gone; otherwise normal period.     */
      uint32_t period = alarmOn ? 1000UL : SENSE_PERIOD_MS;
      if (millis() - lastSenseMs >= period) state = S_READ;
      break;
    }

    /* ---- ERROR: wait, then re-init the whole chain ---------------- */
    case S_ERROR: {
      delay(2000);
      Serial.println(F("[node A] retrying -> INIT"));
      state = S_INIT;
      break;
    }
  }
}
