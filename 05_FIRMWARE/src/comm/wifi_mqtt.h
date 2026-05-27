/**
 * wifi_mqtt.h — WiFi + MQTT NOISYLESS v2.0.0
 * 
 * WiFi   : SSID/pass hardcodés dans include/wifi_config.h
 * MQTT   : host/user/pass dans include/credentials.h
 * Topic  : esp32/noisyless/{MAC}/datas
 * 
 * Usage :
 *   WifiMqtt net;
 *   net.init();              // connecte WiFi + MQTT
 *   net.tick();              // appel régulier (reconnexion auto)
 *   net.publish(payload);   // publie sur le topic
 *   bool ok = net.wifi_ok();
 */

#ifndef WIFI_MQTT_H
#define WIFI_MQTT_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "wifi_config.h"
#include "credentials.h"

#define PUBLISH_INTERVAL_MS  10000UL
#define MQTT_BUFFER_SIZE     1024

class WifiMqtt {
public:
  bool init() {
    // WiFi
    Serial.printf("[WiFi] SSID=%s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    for (int i = 0; i < 120; i++) {  // 30s timeout
      if (WiFi.status() == WL_CONNECTED) break;
      delay(250);
    }
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] FAIL");
      return false;
    }
    
    Serial.printf("[WiFi] IP=%s RSSI=%d\n", 
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    
    // Device ID basé sur l'adresse MAC
    _mac = WiFi.macAddress();
    _mac.replace(":", "");
    _mac.toUpperCase();
    
    // Topic MQTT
    snprintf(_topic, sizeof(_topic), "esp32/noisyless/%s/datas", _mac.c_str());
    Serial.printf("[MQTT] Topic=%s\n", _topic);
    
    // MQTT client
    _mqtt.setClient(_wifiClient);
    _mqtt.setServer(MQTT_HOST, MQTT_PORT);
    _mqtt.setBufferSize(MQTT_BUFFER_SIZE);
    
    return connectMQTT();
  }
  
  /**
   * Maintient les connexions. À appeler dans loop().
   * Retourne true si WiFi + MQTT OK.
   */
  bool tick() {
    // Reconnect WiFi si perdu
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
      for (int i = 0; i < 20; i++) {  // 5s timeout
        if (WiFi.status() == WL_CONNECTED) break;
        delay(250);
      }
    }
    
    // Reconnect MQTT si déconnecté
    if (!_mqtt.connected()) {
      connectMQTT();
    }
    
    _mqtt.loop();
    return (WiFi.status() == WL_CONNECTED) && _mqtt.connected();
  }
  
  bool publish(const char* payload) {
    return _mqtt.publish(_topic, payload);
  }
  
  bool wifi_ok() const { return WiFi.status() == WL_CONNECTED; }
  bool mqtt_ok() { return _mqtt.connected(); }
  
  const char* device_id() const { return _mac.c_str(); }
  const char* topic()    const { return _topic; }
  int         rssi()     const { return WiFi.RSSI(); }

private:
  WiFiClient   _wifiClient;
  PubSubClient _mqtt;
  String       _mac;
  char         _topic[128] = {0};
  
  bool connectMQTT() {
    char cid[64];
    snprintf(cid, sizeof(cid), "nl_%s", _mac.c_str());
    
    Serial.printf("[MQTT] %s:%d...\n", MQTT_HOST, MQTT_PORT);
    if (_mqtt.connect(cid, MQTT_USER, MQTT_PASS)) {
      Serial.println("[MQTT] Connected");
      return true;
    }
    Serial.printf("[MQTT] FAIL rc=%d\n", _mqtt.state());
    return false;
  }
};

#endif // WIFI_MQTT_H
