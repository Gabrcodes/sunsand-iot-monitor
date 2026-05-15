/* =====================================================================
 *  Node A  --  generation / weather sensor node
 *  ESP32 + DHT22 (temperature, humidity) + BH1750 (light) + LoRa Ra-01
 *
 *  Reads the two sensors every SENSE_PERIOD_MS, packs them into a short
 *  CSV line, and transmits it over LoRa to the gateway.
 *
 *  The whole sketch is one explicit finite state machine so it maps 1:1
 *  onto the state diagram in the report:
 *
 *      INIT --ok--> READ --> BUILD --> SEND --> WAIT --(period)--> READ
 *        |                                                          ^
 *        +--fail--> ERROR --(retry)--> INIT                         |
 *                                                                   |
 *      (any sensor read failure in READ also routes to ERROR) -------+
 *
 *  Libraries (install via Arduino IDE Library Manager):
 *    - "LoRa"            by Sandeep Mistry
 *    - "DHT sensor library" by Adafruit  (+ "Adafruit Unified Sensor")
 *    - "BH1750"          by Christopher Laws
 * ===================================================================== */
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <DHT.h>
#include <BH1750.h>
#include "protocol.h"

/* ---- Sensor wiring -------------------------------------------------- */
#define DHT_PIN   4
#define DHT_TYPE  DHT22
/* BH1750 is I2C: SDA = GPIO21, SCL = GPIO22 (ESP32 defaults)            */

DHT     dht(DHT_PIN, DHT_TYPE);
BH1750  lightMeter;

/* ---- State machine ------------------------------------------------- */
enum State { S_INIT, S_READ, S_BUILD, S_SEND, S_WAIT, S_ERROR };
State    state          = S_INIT;
uint32_t seq            = 0;       /* packet counter (lets the gateway   */
                                   /* measure packet loss in testing)    */
uint32_t lastSenseMs    = 0;
char     packet[48];

/* live readings */
float    tempC = NAN, humPct = NAN, lux = NAN;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("[node A] booting"));
}

void loop() {
  switch (state) {

    /* ---- INIT: bring up sensors + radio --------------------------- */
    case S_INIT: {
      dht.begin();
      Wire.begin();                        /* SDA 21, SCL 22            */
      bool okLight = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);

      LoRa.setPins(LORA_PIN_SS, LORA_PIN_RST, LORA_PIN_DIO0);
      bool okRadio = LoRa.begin(LORA_FREQUENCY_HZ);
      if (okRadio) {
        LoRa.setSpreadingFactor(LORA_SPREADING);
        LoRa.setSignalBandwidth(LORA_BANDWIDTH_HZ);
        LoRa.setCodingRate4(LORA_CODING_RATE);
        LoRa.setSyncWord(LORA_SYNC_WORD);
        LoRa.setTxPower(LORA_TX_POWER_DBM);
      }

      if (okLight && okRadio) {
        Serial.println(F("[node A] init OK -> READ"));
        state = S_READ;
      } else {
        Serial.print(F("[node A] init FAIL light="));
        Serial.print(okLight); Serial.print(F(" radio="));
        Serial.println(okRadio);
        state = S_ERROR;
      }
      break;
    }

    /* ---- READ: sample the sensors --------------------------------- */
    case S_READ: {
      tempC  = dht.readTemperature();      /* DHT22: 1 read / 2 s max   */
      humPct = dht.readHumidity();
      lux    = lightMeter.readLightLevel();

      if (isnan(tempC) || isnan(humPct) || lux < 0) {
        Serial.println(F("[node A] sensor read failed -> ERROR"));
        state = S_ERROR;
      } else {
        state = S_BUILD;
      }
      break;
    }

    /* ---- BUILD: format the CSV payload ---------------------------- *
     * Schema:  A,<seq>,<tempC>,<humPct>,<lux>                          */
    case S_BUILD: {
      snprintf(packet, sizeof(packet), "%c,%lu,%.1f,%.1f,%.1f",
               NODE_A_ID, (unsigned long)seq, tempC, humPct, lux);
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
      seq++;
      lastSenseMs = millis();
      state = S_WAIT;
      break;
    }

    /* ---- WAIT: hold until the next sense period ------------------- */
    case S_WAIT: {
      if (millis() - lastSenseMs >= SENSE_PERIOD_MS) state = S_READ;
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
