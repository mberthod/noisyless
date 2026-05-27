/**
 * main.cpp — NOISYLESS Environmental Sensor v2.0.0
 * 
 * ESP32-S3-MINI-1 (N8, sans PSRAM)
 * Architecture modulaire FreeRTOS :
 *   - task_mqtt    : WiFi/MQTT/OTA/publish (cœur 0)
 *   - task_sensors : BME680 + LD2410 + Analog (cœur 1)
 *   - task_led     : timings LED (cœur 0, priorité basse)
 * 
 * Payload MQTT (10s) : lux, mic, BME680 IAQ/CO2/VOC/T/H/P,
 *                       LD2410 présence/distance, RSSI, uptime
 * 
 * LED séquence : 🔴 boot → 🔵 WiFi → 🔵×N version → 🟢 OK
 *                🔴 si WiFi perdu, 🟢 flash 200ms sur publish
 */

#include <Arduino.h>

// ---- Modules NOISYLESS ----
#include "ui/led_status.h"
#include "comm/wifi_mqtt.h"
#include "ota/ota_manager.h"
#include "sensors/bme680_sensor.h"
#include "sensors/ld2410_sensor.h"
#include "sensors/analog_sensors.h"

// ---- Version firmware ----
#define FW_VERSION "2.0.0"

// ---- Objets globaux ----
WifiMqtt       g_net;
OTAManager     g_ota;
AnalogSensors  g_analog;
LD2410Sensor   g_radar;
// g_bme680 déclaré dans bme680_sensor.cpp

// ---- Données partagées entre tâches (protégées par mutex) ----
struct SharedData {
  // BME680
  BME680Readings bme;
  // LD2410
  bool     presence    = false;
  uint16_t distance_cm = 0;
  // Analog
  AnalogReadings analog;
};
static SharedData     g_shared;
static SemaphoreHandle_t g_mutex = nullptr;
static bool           g_wifi_was_ok = false;

// ---- Forward declarations ----
void task_mqtt(void*);
void task_sensors(void*);
void task_led_tick(void*);

// ================================================================
//  SETUP
// ================================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n===== NOISYLESS v%s =====\n", FW_VERSION);
  
  // ---- LED ----
  led_init();
  led_set(LED_RED);  // boot
  
  // ---- WiFi + MQTT ----
  led_set(LED_BLUE);  // tentative WiFi
  if (!g_net.init()) {
    Serial.println("[BOOT] WiFi FAIL — reboot in 5s");
    led_set(LED_RED);
    delay(5000);
    ESP.restart();
  }
  
  // Séquence boot visuelle
  led_boot_sequence(FW_VERSION);
  
  // ---- Capteurs ----
  Serial.println("[INIT] Capteurs...");
  
  // BME680
  if (g_bme680.init()) {
    Serial.println("[BME680] OK");
  } else {
    Serial.println("[BME680] SKIPPED");
  }
  
  // LD2410
  if (g_radar.init()) {
    Serial.println("[LD2410] OK");
  } else {
    Serial.println("[LD2410] SKIPPED");
  }
  
  // Analog
  g_analog.init();
  Serial.println("[ANALOG] OK");
  
  // ---- OTA ----
  g_ota.init(FW_VERSION);
  g_ota.setWifiCheck([]() -> bool { return g_net.wifi_ok(); });
  
  // ---- Mutex partagé ----
  g_mutex = xSemaphoreCreateMutex();
  
  // ---- FreeRTOS Tasks ----
  // Cœur 0 : MQTT + OTA (priorité normale)
  xTaskCreatePinnedToCore(task_mqtt, "mqtt", 4096, nullptr, 1, nullptr, 0);
  // Cœur 1 : Capteurs (priorité élevée pour BSEC2 timing)
  xTaskCreatePinnedToCore(task_sensors, "sensors", 4096, nullptr, 2, nullptr, 1);
  // Cœur 0 : LED timing (priorité basse)
  xTaskCreatePinnedToCore(task_led_tick, "led", 2048, nullptr, 0, nullptr, 0);
  
  Serial.println("[BOOT] Ready");
  led_set(LED_OFF);
  
  // Supprime la tâche setup (loop() inutilisée)
  vTaskDelete(nullptr);
}

void loop() {
  // Jamais atteint — tout est en FreeRTOS
  vTaskDelay(portMAX_DELAY);
}

// ================================================================
//  TASK: MQTT + Publication + OTA (Core 0)
// ================================================================
void task_mqtt(void*) {
  unsigned long lastPub = 0;
  
  for (;;) {
    // Maintien connexion
    bool net_ok = g_net.tick();
    
    // LED rouge si WiFi perdu
    if (!net_ok && g_wifi_was_ok) {
      led_set(LED_RED);
    }
    g_wifi_was_ok = net_ok;
    
    // Publication toutes les 10s
    unsigned long now = millis();
    if (net_ok && now - lastPub >= PUBLISH_INTERVAL_MS) {
      lastPub = now;
      
      // Lit les données partagées
      SharedData snapshot;
      if (g_mutex && xSemaphoreTake(g_mutex, pdMS_TO_TICKS(10))) {
        snapshot = g_shared;
        xSemaphoreGive(g_mutex);
      }
      
      // Construit le payload JSON
      char payload[768];
      const BME680Readings& b = snapshot.bme;
      
      if (b.valid) {
        snprintf(payload, sizeof(payload),
          "{"
          "\"product\":\"noisyless_env\","
          "\"client_code\":\"client_demo\","
          "\"device_id\":\"%s\","
          "\"fw_version\":\"%s\","
          "\"lux1_raw\":%u,\"lux2_raw\":%u,"
          "\"sound_mic1_pp\":%u,\"sound_mic2_pp\":%u,"
          "\"presence\":%s,\"distance_cm\":%u,"
          "\"temp_comp_c\":%.2f,\"hum_comp_pct\":%.2f,\"pressure_pa\":%u,"
          "\"iaq\":%.1f,\"co2eq_ppm\":%.1f,\"bvoc_eq_ppm\":%.2f,"
          "\"gas_ohms\":%u,\"iaq_accuracy\":%u,"
          "\"stab_status\":%u,\"run_in_status\":%u,"
          "\"rssi_dbm\":%d,\"uptime_s\":%lu"
          "}",
          g_net.device_id(), FW_VERSION,
          snapshot.analog.lux1_raw, snapshot.analog.lux2_raw,
          snapshot.analog.mic1_pp, snapshot.analog.mic2_pp,
          snapshot.presence ? "true" : "false", snapshot.distance_cm,
          b.temp_comp_c, b.hum_comp_pct, b.pressure_pa,
          b.iaq, b.co2eq_ppm, b.bvoc_eq_ppm,
          b.gas_ohms, b.iaq_accuracy,
          b.stab_status, b.run_in_status,
          g_net.rssi(), millis() / 1000
        );
      } else {
        // Payload sans BME680
        snprintf(payload, sizeof(payload),
          "{"
          "\"product\":\"noisyless_env\","
          "\"client_code\":\"client_demo\","
          "\"device_id\":\"%s\","
          "\"fw_version\":\"%s\","
          "\"lux1_raw\":%u,\"lux2_raw\":%u,"
          "\"sound_mic1_pp\":%u,\"sound_mic2_pp\":%u,"
          "\"presence\":%s,\"distance_cm\":%u,"
          "\"rssi_dbm\":%d,\"uptime_s\":%lu"
          "}",
          g_net.device_id(), FW_VERSION,
          snapshot.analog.lux1_raw, snapshot.analog.lux2_raw,
          snapshot.analog.mic1_pp, snapshot.analog.mic2_pp,
          snapshot.presence ? "true" : "false", snapshot.distance_cm,
          g_net.rssi(), millis() / 1000
        );
      }
      
      // Publie + flash LED
      if (g_net.publish(payload)) {
        led_flash_green();
      }
    }
    
    // OTA check (géré par tick() avec son propre timer 60s)
    g_ota.tick();
    
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ================================================================
//  TASK: Capteurs (Core 1) — lit BME680 + LD2410 + Analog
// ================================================================
void task_sensors(void*) {
  for (;;) {
    // BME680 (BSEC2 run — doit être appelé régulièrement)
    if (g_bme680.initialized()) {
      g_bme680.tick();
    }
    
    // LD2410
    if (g_radar.ok()) {
      g_radar.tick();
    }
    
    // Analog (bloque 50ms pour échantillonnage micros)
    const AnalogReadings& a = g_analog.read();
    
    // Écrit dans la zone partagée
    if (g_mutex && xSemaphoreTake(g_mutex, pdMS_TO_TICKS(10))) {
      g_shared.bme         = g_bme680.last();
      g_shared.presence    = g_radar.presence();
      g_shared.distance_cm = g_radar.distance_cm();
      g_shared.analog      = a;
      xSemaphoreGive(g_mutex);
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ================================================================
//  TASK: LED timings (Core 0, priorité basse)
// ================================================================
void task_led_tick(void*) {
  for (;;) {
    // Gère les timings (vert boot, flash publish)
    led_tick();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
