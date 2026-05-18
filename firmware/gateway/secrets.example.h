/* secrets.example.h -- template. Copy to secrets.h (same folder) and
 * fill in real values. secrets.h is gitignored; this file is committed.
 *
 *   cp secrets.example.h secrets.h    (then edit secrets.h)
 *
 * WIFI_SSID is matched byte-for-byte. If your hotspot name contains a
 * typographic apostrophe (iOS uses U+2019 ’, not ASCII '), paste the
 * exact character or the join will silently fail. */
#ifndef SUNSAND_SECRETS_H
#define SUNSAND_SECRETS_H

#define WIFI_SSID     "your-wifi-ssid"
#define WIFI_PASS     "your-wifi-password"
#define CLOUD_URL     "http://your-host:8080/ingest"
#define INGEST_TOKEN  "your-ingest-token"

#endif /* SUNSAND_SECRETS_H */
