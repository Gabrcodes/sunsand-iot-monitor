/* Throwaway diagnostic: scan + dump all 2.4 GHz WiFi the ESP32 can see.
 * Flash this to the gateway ESP32, watch serial @115200, then reflash
 * the real gateway sketch. NOTE: ESP32-WROOM is 2.4 GHz only -- a 5 GHz
 * AP (e.g. an iPhone hotspot with "Maximize Compatibility" OFF) will
 * NOT appear here at all. */
#include <WiFi.h>

/* set this to your hotspot SSID to highlight it in the scan */
#define LOOK_FOR "your-hotspot-ssid"

void setup() {
  Serial.begin(115200);
  delay(300);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(200);
  Serial.println(F("[scan] ready (2.4 GHz only)"));
}

void loop() {
  Serial.println(F("[scan] scanning..."));
  int n = WiFi.scanNetworks();
  if (n <= 0) {
    Serial.println(F("[scan] NO networks found"));
  } else {
    bool found = false;
    for (int i = 0; i < n; i++) {
      Serial.printf("%2d  %-32s  ch%2d  %4d dBm  %s\n",
        i + 1, WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i),
        WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "secured");
      if (WiFi.SSID(i) == LOOK_FOR) found = true;
    }
    Serial.println(found
      ? F("[scan] >>> '" LOOK_FOR "' IS visible (2.4 GHz, joinable)")
      : F("[scan] >>> '" LOOK_FOR "' NOT seen -> likely 5 GHz; turn ON 'Maximize Compatibility' on the iPhone"));
  }
  WiFi.scanDelete();
  delay(5000);
}
