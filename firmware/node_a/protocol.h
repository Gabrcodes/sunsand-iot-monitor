/* =====================================================================
 * protocol.h  --  shared definitions for the SunSand IoT monitor
 *
 * One sensor node (A) transmits a short CSV line over LoRa. The demo is
 * Gateway + Node A only. The gateway receives those lines, attaches link
 * quality, and re-emits one clean JSON line per packet over USB serial
 * to the laptop dashboard.
 *
 * Keeping the over-the-air payload as plain CSV (not JSON) means the
 * tiny ESP32 node never needs a JSON library and the packet stays well
 * under the LoRa SF7 payload limit. The JSON is built once, on the
 * gateway, where it is convenient for the dashboard to consume.
 * ===================================================================== */
#ifndef SUNSAND_PROTOCOL_H
#define SUNSAND_PROTOCOL_H

/* ---- Radio link parameters (identical on both boards) -------------- */
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

/* ---- Node identifier ----------------------------------------------- */
#define NODE_A_ID  'A'   /* environmental node: DHT22 + photoresistor    */

/* ---- Telemetry period --------------------------------------------- */
#define SENSE_PERIOD_MS  3000UL   /* one reading + transmit every 3 s     */

/* ---- Transport selector -------------------------------------------- *
 * 1 = send telemetry over LoRa (normal 2-board demo).
 * 0 = print the CSV straight to USB serial instead (fallback path: plug
 *     the node into the laptop if LoRa misbehaves on demo day).
 * The dashboard accepts the same CSV/JSON either way.                   */
#ifndef USE_LORA
#define USE_LORA  1
#endif

/* CSV schema (documented here so both boards agree):
 *   Node A:  A,<seq>,<tempC>,<humPct>,<light>,<night 0|1>,<alarm 0|1>
 * where <light> is the raw 12-bit photoresistor ADC value (0..4095;
 * uncalibrated), <night> is 1 when it is dark, and <alarm> is the
 * environment-safety bonus flag (overheat, or dark + high humidity).
 * The gateway turns this into:
 *   {"node":"A","seq":12,"temp":24.5,"hum":45.0,"light":1830,
 *    "night":0,"alarm":0,"rssi":-65,"snr":9.2,"t_ms":12345}
 */

#endif /* SUNSAND_PROTOCOL_H */
