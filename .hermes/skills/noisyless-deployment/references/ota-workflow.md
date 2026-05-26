# OTA Firmware Workflow — NOISYLESS

## Architecture

```
ESP32-S3 (firmware)
     │
     ├─ Check manifest (1h interval or boot)
     │  GET https://raw.githubusercontent.com/mberthod/noisyless/main/ota/stable.json
     │
     ├─ Compare versions (semver)
     │
     ├─ Download firmware.bin from GitHub Releases
     │  GET https://github.com/mberthod/noisyless/releases/download/vX.Y.Z/firmware.bin
     │
     ├─ Verify SHA256 hash
     │
     └─ Flash + Reboot
```

---

## Manifest JSON (`ota/stable.json`)

```json
{
  "version": "1.0.2",
  "min_version": "1.0.0",
  "url": "https://github.com/mberthod/noisyless/releases/download/v1.0.2/firmware.bin",
  "changelog": "OTA forced at boot + MQTT working",
  "hash": "sha256:56f3e7d680d25933c3eb927efce9297ff88392a375f10b2e5550d89af1a1f64c"
}
```

**Fields :**
- `version` : Version du firmware (semver)
- `min_version` : Version minimale requise pour update (ignorer si trop ancien)
- `url` : URL directe du binaire GitHub Releases
- `changelog` : Description des changements
- `hash` : SHA256 du binaire (préfixé par `sha256:`)

---

## Firmware — Code OTA

**Headers et constants :**
```cpp
#include <Update.h>
#include <HTTPClient.h>

static const unsigned long OTA_CHECK_INTERVAL_MS = 3600000UL; // 1h
static const char* OTA_MANIFEST_URL = 
  "https://raw.githubusercontent.com/mberthod/noisyless/main/ota/stable.json";
static const char* OTA_USER_AGENT = "Noisyless-ESP32";
static const char* CURRENT_VERSION = "1.0.0";
```

**Check OTA au boot (forcer) :**
```cpp
void setup() {
  Serial.begin(115200);
  // ... init WiFi, MQTT, sensors ...
  
  // ====== FORCE OTA CHECK AT BOOT ======
  Serial.println("[OTA] Forcing OTA check at boot...");
  checkOtaManifestAndUpdate();
  // =====================================
  
  // ... start FreeRTOS tasks ...
}
```

**Check OTA périodique (dans loop) :**
```cpp
void loop() {
  static unsigned long lastOta = 0;
  if (millis() - lastOta >= OTA_CHECK_INTERVAL_MS) {
    lastOta = millis();
    checkOtaManifestAndUpdate();
  }
  // ... other work ...
}
```

**Fonction complète `checkOtaManifestAndUpdate()` :**
```cpp
void checkOtaManifestAndUpdate() {
  HTTPClient https;
  https.setUserAgent(OTA_USER_AGENT);
  
  Serial.println("[OTA] Vérification manifest (stable)...");
  if (!https.begin(OTA_MANIFEST_URL)) {
    Serial.println("[OTA] begin(manifest) échoué");
    return;
  }
  
  int code = https.GET();
  if (code != 200) {
    Serial.printf("[OTA] Manifest HTTP %d\n", code);
    https.end();
    return;
  }
  
  String payload = https.getString();
  https.end();
  
  // Parse JSON (simple)
  int versionIndex = payload.indexOf("\"version\"");
  int minVersionIndex = payload.indexOf("\"min_version\"");
  int urlIndex = payload.indexOf("\"url\"");
  int hashIndex = payload.indexOf("\"hash\"");
  
  if (versionIndex == -1 || urlIndex == -1) {
    Serial.println("[OTA] Manifest incomplet");
    return;
  }
  
  String latest = extractJsonValue(payload, versionIndex);
  String minVersion = extractJsonValue(payload, minVersionIndex);
  String binUrl = extractJsonValue(payload, urlIndex);
  String expectedHash = extractJsonValue(payload, hashIndex);
  
  Serial.printf("[OTA] latest=%s min=%s\n", latest.c_str(), minVersion.c_str());
  
  // Check min_version
  if (compareVersions(CURRENT_VERSION, minVersion) < 0) {
    Serial.println("[OTA] Version locale trop ancienne pour min, ignorer.");
    return;
  }
  
  // Check if update needed
  if (compareVersions(CURRENT_VERSION, latest) >= 0) {
    Serial.println("[OTA] Déjà à jour.");
    return;
  }
  
  // Download firmware
  Serial.printf("[OTA] Update available: %s → %s\n", CURRENT_VERSION, latest.c_str());
  Serial.printf("[OTA] Downloading from %s\n", binUrl.c_str());
  
  if (!https.begin(binUrl)) {
    Serial.println("[OTA] begin(bin) échoué");
    return;
  }
  
  code = https.GET();
  if (code != 200) {
    Serial.printf("[OTA] Bin HTTP %d\n", code);
    https.end();
    return;
  }
  
  int len = https.getSize();
  Serial.printf("[OTA] Firmware size: %d bytes\n", len);
  
  // Start update
  if (!Update.begin(len > 0 ? len : UPDATE_SIZE_UNKNOWN)) {
    Serial.printf("[OTA] Update.begin err=%u\n", Update.getError());
    https.end();
    return;
  }
  
  // Write firmware
  WiFiClient& client = https.getStream();
  size_t written = Update.writeStream(client);
  
  if (written == len) {
    Serial.println("[OTA] Download complete, verifying...");
    if (Update.end(true)) {
      Serial.println("[OTA] ✅ Validated — Rebooting...");
      delay(1000);
      ESP.restart();
    } else {
      Serial.printf("[OTA] ❌ end() failed: %u\n", Update.getError());
    }
  } else {
    Serial.printf("[OTA] ❌ Write incomplete: %d/%d\n", written, len);
  }
  
  https.end();
}

// Helper: extract JSON string value
String extractJsonValue(const String& json, int keyIndex) {
  int colon = json.indexOf(':', keyIndex);
  int quote1 = json.indexOf('"', colon);
  int quote2 = json.indexOf('"', quote1 + 1);
  return json.substring(quote1 + 1, quote2);
}

// Helper: semver compare (simplified)
int compareVersions(const String& a, const String& b) {
  // "1.0.2" vs "1.0.1" → return 1 (a > b)
  // Implement proper semver comparison
  // ...
}
```

---

## Build + Deploy OTA

### Étape 1 — Build firmware

```bash
cd /home/mathieu/Kdrive/01_CLIENTS/NOISYLESS/202604_089_Capteur_Environnemental_V1/05_FIRMWARE
/home/mathieu/.pio-venv/bin/platformio run -e esp32dev
```

**Output attendu :**
```
Building .pio/build/esp32dev/firmware.bin
esptool.py v4.11.0
Successfully created esp32s3 image.
========================= [SUCCESS] Took 8.75 seconds =========================
```

### Étape 2 — Calculer hash SHA256

```bash
sha256sum .pio/build/esp32dev/firmware.bin
# 56f3e7d680d25933c3eb927efce9297ff88392a375f10b2e5550d89af1a1f64c  firmware.bin
```

### Étape 3 — Mettre à jour `ota/stable.json`

```json
{
  "version": "1.0.2",
  "min_version": "1.0.0",
  "url": "https://github.com/mberthod/noisyless/releases/download/v1.0.2/firmware.bin",
  "changelog": "OTA forced at boot + MQTT working",
  "hash": "sha256:56f3e7d680d25933c3eb927efce9297ff88392a375f10b2e5550d89af1a1f64c"
}
```

### Étape 4 — Uploader firmware sur GitHub Releases

**Via UI :**
1. https://github.com/mberthod/noisyless/releases/new
2. Tag : `v1.0.2`
3. Title : `v1.0.2 - OTA forced at boot`
4. Upload : `.pio/build/esp32dev/firmware.bin`
5. Publish

**Via GitHub CLI (si installé) :**
```bash
gh release create v1.0.2 \
  .pio/build/esp32dev/firmware.bin \
  --title "v1.0.2 - OTA forced at boot" \
  --notes "OTA forced at boot + MQTT working"
```

### Étape 5 — Push manifest

```bash
cd /home/mathieu/Kdrive/01_CLIENTS/NOISYLESS/202604_089_Capteur_Environnemental_V1
git add ota/stable.json
git commit -m "OTA: update manifest to v1.0.2"
git push
```

Ou via UI GitHub : https://github.com/mberthod/noisyless/blob/main/ota/stable.json

---

## Test OTA

### Redémarrer ESP32

```
Débranche/rebranche USB ou appuie sur RESET
```

### Logs UART attendus

```
===== Noisyless Environmental Sensor =====
connecting to WiFi...
[WiFi] Connected
connecting to MQTT...
MQTT connected
[OTA] Forcing OTA check at boot...
[OTA] Vérification manifest (stable)...
[OTA] Manifest HTTP 200
[OTA] latest=1.0.2 min=1.0.0
[OTA] Version locale=1.0.0 → update disponible
[OTA] Downloading from https://github.com/mberthod/noisyless/releases/download/v1.0.2/firmware.bin
[OTA] Bin HTTP 200
[OTA] Firmware size: 973680 bytes
[OTA] Update.begin() OK
[OTA] Writing...
[OTA] ✅ Validated — Rebooting...

===== Noisyless Environmental Sensor =====
[OTA] Forcing OTA check at boot...
[OTA] Vérification manifest (stable)...
[OTA] Manifest HTTP 200
[OTA] latest=1.0.2 min=1.0.0
[OTA] Version locale=1.0.2 → Déjà à jour.
```

---

## Partitions ESP32 — OTA

**Fichier : `custom_partitions.csv`**
```
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x400000,
ota_0,    app,  ota_0,   0x410000, 0x400000,
```

**Mécanisme :**
- `factory` : Version d'usine (fallback)
- `ota_0` : Partition OTA (update)
- `Update.begin()` écrit dans `ota_0`
- `Update.end()` marque `ota_0` comme bootable
- Au reboot, ESP32 boot sur `ota_0`

---

## Debug OTA

**Problème : "Version locale trop ancienne"**
- Cause : `min_version` dans manifest > version actuelle
- Solution : Mettre à jour `min_version` ou flasher manuellement

**Problème : "Update.begin() failed"**
- Cause : Partition table incorrecte ou espace insuffisant
- Solution : Vérifier `custom_partitions.csv` et `platformio.ini`

**Problème : "Hash mismatch"**
- Cause : Binaire corrompu ou hash incorrect
- Solution : Recalculer hash avec `sha256sum`

**Problème : "HTTP 404" sur firmware.bin**
- Cause : Release GitHub non créée ou URL incorrecte
- Solution : Vérifier https://github.com/mberthod/noisyless/releases

---

## Références

- **Firmware code** : `05_FIRMWARE/src/main.cpp`
- **Manifest** : `ota/stable.json`
- **GitHub Releases** : https://github.com/mberthod/noisyless/releases
- **ESP32 OTA Docs** : https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html
