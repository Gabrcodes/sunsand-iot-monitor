/* =====================================================================
 * protocol.h  --  shared definitions for the SunSand IoT monitor
 *
 * Two sensor nodes (A, B) transmit a short CSV line over LoRa. The
 * gateway receives those lines, attaches link quality, and re-emits one
 * clean JSON line per packet over USB serial to the laptop dashboard.
 *
 * Keeping the over-the-air payload as plain CSV (not JSON) means the
 * tiny ESP32 nodes never need a JSON library and the packet stays well
 * under the LoRa SF7 payload limit. The JSON is built once, on the
 * gateway, where it is convenient for the dashboard to consume.
 * ===================================================================== */
#ifndef SUNSAND_PROTOCOL_H
#define SUNSAND_PROTOCOL_H

/* ---- Radio link parameters (identical on all three boards) ---------- */
#define LORA_FREQUENCY_HZ   433E6   /* 433 MHz ISM band                  */
#define LORA_SPREADING      7       /* SF7  -> fast, short range, fine    */
#define LORA_BANDWIDTH_HZ   125E3   /* 125 kHz                            */
#define LORA_CODING_RATE    5       /* 4/5                                */
#define LORA_SYNC_WORD      0x12    /* private network id                 */
#define LORA_TX_POWER_DBM   17      /* drop to 10 if boards are <1 m apart */

/* ---- LoRa SX1278 (Ra-01) wiring to the ESP32 ----------------------- *
 * SPI bus is fixed by the ESP32 VSPI peripheral:
 *   SCK  = GPIO18    MISO = GPIO19    MOSI = GPIO23
 * The three control lines below are free choices kept off strapping pins */
#define LORA_PIN_SS    5
#define LORA_PIN_RST   27
#define LORA_PIN_DIO0  26

/* ---- Node identifiers ---------------------------------------------- */
#define NODE_A_ID  'A'   /* generation / weather  : DHT22 + BH1750       */
#define NODE_B_ID  'B'   /* environment / security: DHT22 + PIR + reed   */

/* ---- Telemetry period --------------------------------------------- */
#define SENSE_PERIOD_MS  3000UL   /* one reading + transmit every 3 s     */

/* ---- Transport selector -------------------------------------------- *
 * 1 = send telemetry over LoRa (normal 3-board demo).
 * 0 = print the CSV straight to USB serial instead (fallback path: plug
 *     a single node into the laptop if LoRa misbehaves on demo day).
 * The dashboard accepts the same CSV/JSON either way.                   */
#ifndef USE_LORA
#define USE_LORA  1
#endif

/* CSV schemas (documented here so every board agrees):
 *   Node A:  A,<seq>,<tempC>,<humPct>,<lux>
 *   Node B:  B,<seq>,<tempC>,<humPct>,<pir 0|1>,<door 0|1>,<alarm 0|1>
 * The gateway turns these into:
 *   {"node":"A","seq":12,"temp":24.5,"hum":45.0,"lux":320.0,
 *    "rssi":-65,"snr":9.2,"t_ms":12345}
 */

#endif /* SUNSAND_PROTOCOL_H */
