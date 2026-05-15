/* =====================================================================
 *  Node B  --  environment / security sensor node
 *  ESP32 + DHT22 + HC-SR501 PIR + magnetic reed switch + buzzer
 *
 *  Reports temperature, humidity, motion and door state. Implements a
 *  small security rule as a BONUS feature: if motion is detected WHILE
 *  the door is open, it enters an ALARM state and sounds the buzzer.
 *
 *  State machine (note the ALARM branch -- this is the bonus state):
 *
 *      INIT --ok--> READ --> EVAL --+--(safe)---> BUILD --> SEND --> WAIT
 *        |                          |                                 |
 *        |                          +--(motion & door open)--> ALARM -+
 *        |                                (buzzer on, alarm=1)         |
 *        +--fail--> ERROR --(retry)--> INIT      WAIT --(period)--> READ
 *
 *  Libraries: "LoRa" (Sandeep Mistry), "DHT sensor library" (Adafruit)
 *             + "Adafruit Unified Sensor".
 * ===================================================================== */
#include <SPI.h>
#include <LoRa.h>
#include <DHT.h>
#include "protocol.h"

/* ---- Sensor / actuator wiring -------------------------------------- */
#define DHT_PIN    4
#define DHT_TYPE   DHT22
#define PIR_PIN    34          /* HC-SR501 OUT (input-only pin is fine)  */
#define REED_PIN   32          /* reed to GND, uses INPUT_PULLUP         */
#define BUZZER_PIN 25          /* active buzzer: HIGH = beep             */

DHT dht(DHT_PIN, DHT_TYPE);

/* ---- State machine ------------------------------------------------- */
enum State { S_INIT, S_READ, S_EVAL, S_ALARM, S_BUILD, S_SEND, S_WAIT, S_ERROR };
State    state       = S_INIT;
uint32_t seq         = 0;
uint32_t lastSenseMs = 0;
char     packet[48];

/* live readings */
float tempC = NAN, humPct = NAN;
int   pir = 0, doorOpen = 0, alarm = 0;

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);
  pinMode(REED_PIN, INPUT_PULLUP);   /* door shut (magnet) -> LOW        */
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  delay(200);
  Serial.println(F("[node B] booting"));
}

void loop() {
  switch (state) {

    case S_INIT: {
      dht.begin();
      LoRa.setPins(LORA_PIN_SS, LORA_PIN_RST, LORA_PIN_DIO0);
      bool okRadio = LoRa.begin(LORA_FREQUENCY_HZ);
      if (okRadio) {
        LoRa.setSpreadingFactor(LORA_SPREADING);
        LoRa.setSignalBandwidth(LORA_BANDWIDTH_HZ);
        LoRa.setCodingRate4(LORA_CODING_RATE);
        LoRa.setSyncWord(LORA_SYNC_WORD);
        LoRa.setTxPower(LORA_TX_POWER_DBM);
        Serial.println(F("[node B] init OK -> READ"));
        state = S_READ;
      } else {
        Serial.println(F("[node B] LoRa init FAIL -> ERROR"));
        state = S_ERROR;
      }
      break;
    }

    /* ---- READ: sample everything ---------------------------------- */
    case S_READ: {
      tempC    = dht.readTemperature();
      humPct   = dht.readHumidity();
      pir      = (digitalRead(PIR_PIN) == HIGH) ? 1 : 0;
      doorOpen = (digitalRead(REED_PIN) == HIGH) ? 1 : 0;  /* HIGH = open */

      if (isnan(tempC) || isnan(humPct)) {
        Serial.println(F("[node B] DHT read failed -> ERROR"));
        state = S_ERROR;
      } else {
        state = S_EVAL;
      }
      break;
    }

    /* ---- EVAL: the security rule (bonus) -------------------------- */
    case S_EVAL: {
      if (pir == 1 && doorOpen == 1) {
        state = S_ALARM;
      } else {
        alarm = 0;
        digitalWrite(BUZZER_PIN, LOW);
        state = S_BUILD;
      }
      break;
    }

    /* ---- ALARM: motion while the door is open --------------------- */
    case S_ALARM: {
      alarm = 1;
      digitalWrite(BUZZER_PIN, HIGH);      /* sound the buzzer          */
      Serial.println(F("[node B] !! ALARM: motion + door open !!"));
      state = S_BUILD;                     /* still report the packet   */
      break;
    }

    /* ---- BUILD: CSV payload --------------------------------------- *
     * Schema:  B,<seq>,<tempC>,<humPct>,<pir>,<door>,<alarm>           */
    case S_BUILD: {
      snprintf(packet, sizeof(packet), "%c,%lu,%.1f,%.1f,%d,%d,%d",
               NODE_B_ID, (unsigned long)seq, tempC, humPct,
               pir, doorOpen, alarm);
      state = S_SEND;
      break;
    }

    case S_SEND: {
#if USE_LORA
      LoRa.beginPacket();
      LoRa.print(packet);
      LoRa.endPacket();
#else
      Serial.println(packet);
#endif
      Serial.print(F("[node B] sent: ")); Serial.println(packet);
      seq++;
      lastSenseMs = millis();
      state = S_WAIT;
      break;
    }

    case S_WAIT: {
      /* In alarm we re-evaluate quickly so the buzzer clears fast once
       * the intruder leaves or the door shuts; otherwise normal period. */
      uint32_t period = alarm ? 500UL : SENSE_PERIOD_MS;
      if (millis() - lastSenseMs >= period) state = S_READ;
      break;
    }

    case S_ERROR: {
      digitalWrite(BUZZER_PIN, LOW);
      delay(2000);
      Serial.println(F("[node B] retrying -> INIT"));
      state = S_INIT;
      break;
    }
  }
}
