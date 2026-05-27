/**
 * ota_manager.h — OTA Over-The-Air NOISYLESS v2.0.0
 * 
 * Mécanisme : check HTTP manifest toutes les 60s,
 *             si version ≠ FW_VERSION → download + flash + reboot.
 * 
 * URL    : http://91.99.26.43/firmware/latest.json
 * Format : {"version":"2.0.0","url":"http://.../firmware.bin","sha256":"..."}
 * 
 * Usage :
 *   OTAManager ota;
 *   ota.init("2.0.0");
 *   ota.tick();  // appel régulier
 */

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <mbedtls/sha256.h>

#define OTA_INTERVAL_MS       60000UL  // 60s
#define OTA_MANIFEST_URL      "http://91.99.26.43/firmware/latest.json"
#define OTA_HTTP_TIMEOUT_MS   10000    // 10s manifest
#define OTA_DOWNLOAD_TIMEOUT_MS 30000  // 30s download

class OTAManager {
public:
  void init(const char* current_version) {
    _version = current_version;
    _last_check = millis();
    Serial.printf("[OTA] Current=%s, check every %lus\n", _version, OTA_INTERVAL_MS/1000);
  }
  
  /**
   * Vérifie si une mise à jour est disponible.
   * Bloquant si OTA déclenché (10-30s).
   */
  void tick() {
    if (!_wifi_ok_fn || !_wifi_ok_fn()) return;
    
    unsigned long now = millis();
    if (now - _last_check < OTA_INTERVAL_MS) return;
    _last_check = now;
    
    checkForUpdate();
  }
  
  /**
   * Callback pour vérifier l'état WiFi sans couplage.
   */
  void setWifiCheck(bool (*fn)()) { _wifi_ok_fn = fn; }

private:
  const char*  _version = nullptr;
  unsigned long _last_check = 0;
  bool (*_wifi_ok_fn)() = nullptr;
  
  // ---- Helpers ----
  static String _toHex(const uint8_t* buf, size_t len) {
    const char* hex = "0123456789abcdef";
    String s;
    s.reserve(len * 2);
    for (size_t i = 0; i < len; i++) {
      s += hex[(buf[i] >> 4) & 0xF];
      s += hex[buf[i] & 0xF];
    }
    return s;
  }
  
  static String _jsonExtract(const String& body, const char* key) {
    String search = "\"" + String(key) + "\"";
    int i = body.indexOf(search);
    if (i < 0) return "";
    i = body.indexOf(":", i);
    if (i < 0) return "";
    i = body.indexOf("\"", i);
    if (i < 0) return "";
    int j = body.indexOf("\"", i + 1);
    if (j < 0) return "";
    return body.substring(i + 1, j);
  }
  
  void checkForUpdate() {
    HTTPClient http;
    http.setTimeout(OTA_HTTP_TIMEOUT_MS);
    http.begin(OTA_MANIFEST_URL);
    
    int code = http.GET();
    if (code != 200) { http.end(); return; }
    
    String body = http.getString();
    http.end();
    
    String latest     = _jsonExtract(body, "version");
    String url        = _jsonExtract(body, "url");
    String sha256hex  = _jsonExtract(body, "sha256");
    sha256hex.toLowerCase();
    
    if (!latest.length() || !url.length() || !sha256hex.length()) return;
    if (latest == _version) return;  // déjà à jour
    
    Serial.printf("[OTA] v%s → v%s\n", _version, latest.c_str());
    downloadAndFlash(url, sha256hex);
  }
  
  void downloadAndFlash(const String& url, const String& sha256hex) {
    HTTPClient dl;
    dl.setTimeout(OTA_DOWNLOAD_TIMEOUT_MS);
    dl.begin(url);
    
    if (dl.GET() != 200) { dl.end(); return; }
    
    int len = dl.getSize();
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0);
    
    if (!Update.begin(len > 0 ? len : UPDATE_SIZE_UNKNOWN)) {
      dl.end();
      mbedtls_sha256_free(&ctx);
      Serial.println("[OTA] Update.begin failed");
      return;
    }
    
    WiFiClient* stream = dl.getStreamPtr();
    uint8_t buf[1024];
    size_t total = 0;
    
    while (dl.connected() && (len > 0 || len == -1)) {
      size_t avail = stream->available();
      if (!avail) { delay(1); continue; }
      size_t rd = stream->readBytes(buf, avail > sizeof(buf) ? sizeof(buf) : avail);
      if (!rd) break;
      total += rd;
      mbedtls_sha256_update_ret(&ctx, buf, rd);
      if (Update.write(buf, rd) != rd) { Update.abort(); break; }
      if (len > 0) len -= rd;
    }
    
    dl.end();
    uint8_t digest[32];
    mbedtls_sha256_finish_ret(&ctx, digest);
    mbedtls_sha256_free(&ctx);
    
    String got = _toHex(digest, 32);
    got.toLowerCase();
    if (got != sha256hex) {
      Serial.println("[OTA] SHA256 mismatch");
      Update.abort();
      return;
    }
    
    if (!Update.end() || !Update.isFinished()) {
      Serial.println("[OTA] Flash failed");
      return;
    }
    
    Serial.println("[OTA] OK — rebooting");
    delay(200);
    ESP.restart();
  }
};

#endif // OTA_MANAGER_H
