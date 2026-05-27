/**
 * bme680_sensor.h — BME680 + BSEC2 NOISYLESS v2.0.0
 * 
 * Pins I2C  : SDA=GPIO35, SCL=GPIO36 (schéma RevB)
 * Mode      : Ultra Low Power (ULP) — 3s entre mesures
 * Sorties   : IAQ, CO2eq, bVOCeq, T°/Hum/Pression compensées,
 *             Gas, Raw T/H/G, stab/run-in status
 * 
 * Usage :
 *   BME680Sensor bme;
 *   bme.init();             // scan @0x76 puis @0x77
 *   bme.tick();             // appel régulier (depuis task FreeRTOS)
 *   BME680Readings data = bme.last();
 * 
 * NB : BSEC2 nécessite des appels réguliers à tick() pour
 *      maintenir la calibration IAQ (toutes les 3s en ULP).
 */

#ifndef BME680_SENSOR_H
#define BME680_SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include <bsec2.h>

// Pins I2C (schéma RevB)
#define PIN_BME_INT  34   // pas utilisé en mode polling
#define PIN_BME_MODE 37   // LOW = mode normal
#define PIN_I2C_SDA  35
#define PIN_I2C_SCL  36

// Nombre total de sorties BSEC demandées
#define BSEC_OUTPUT_COUNT 13

struct BME680Readings {
  bool  valid          = false;
  // Valeurs compensées
  float temp_comp_c    = 0.0f;
  float hum_comp_pct   = 0.0f;
  uint32_t pressure_pa = 0;
  // Qualité d'air
  float iaq            = 0.0f;   // 0-500 (0=clean)
  float s_iaq          = 0.0f;   // static IAQ
  float co2eq_ppm      = 0.0f;
  float bvoc_eq_ppm    = 0.0f;
  uint8_t iaq_accuracy = 0;      // 0..3 (3=max)
  // Gas
  uint32_t gas_ohms    = 0;
  float gas_pct        = 0.0f;
  // Raw
  float raw_temp_c     = 0.0f;
  float raw_hum_pct    = 0.0f;
  uint32_t raw_gas_ohms = 0;
  // Statut
  uint8_t stab_status   = 0;     // 0=ongoing, 1=complete
  uint8_t run_in_status = 0;     // 0=ongoing, 1=complete
};

class BME680Sensor {
public:
  bool init() {
    // Config pins BME
    pinMode(PIN_BME_INT, OUTPUT);
    digitalWrite(PIN_BME_INT, HIGH);
    pinMode(PIN_BME_MODE, OUTPUT);
    digitalWrite(PIN_BME_MODE, LOW);
    
    // I2C sur pins dédiés
    Wire.setPins(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.begin();
    delay(50);
    
    // Scan I2C — le BME680 répond à 0x76 ou 0x77
    Wire.beginTransmission(0x76);
    if (Wire.endTransmission() != 0) {
      Wire.beginTransmission(0x77);
      if (Wire.endTransmission() != 0) {
        Serial.println("[BME680] Introuvable sur le bus I2C");
        return false;
      }
    }
    Serial.println("[BME680] Détecté sur I2C");
    
    // Liste des sorties BSEC demandées
    bsecSensor sensorList[BSEC_OUTPUT_COUNT] = {
      BSEC_OUTPUT_IAQ,
      BSEC_OUTPUT_STATIC_IAQ,
      BSEC_OUTPUT_CO2_EQUIVALENT,
      BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
      BSEC_OUTPUT_RAW_PRESSURE,
      BSEC_OUTPUT_RAW_GAS,
      BSEC_OUTPUT_GAS_PERCENTAGE,
      BSEC_OUTPUT_RAW_TEMPERATURE,
      BSEC_OUTPUT_RAW_HUMIDITY,
      BSEC_OUTPUT_STABILIZATION_STATUS,
      BSEC_OUTPUT_RUN_IN_STATUS,
    };
    
    if (!_bsec.begin(BME68X_I2C_ADDR_LOW, Wire)) {
      Serial.println("[BME680] BSEC2 init failed");
      return false;
    }
    
    // Config mode Ultra Low Power
    if (!_bsec.updateSubscription(sensorList, BSEC_OUTPUT_COUNT, BSEC_SAMPLE_RATE_ULP)) {
      Serial.println("[BME680] BSEC2 subscription failed");
      return false;
    }
    
    _bsec.attachCallback(newDataCallback_static);
    
    Serial.printf("[BME680] BSEC2 v%d.%d initialized (ULP mode)\n", 
                  _bsec.version.major, _bsec.version.minor);
    _initialized = true;
    return true;
  }
  
  /**
   * À appeler régulièrement (~chaque loop ou task 100ms).
   * BSEC2 gère son propre timing interne.
   */
  void tick() {
    if (!_initialized) return;
    _bsec.run();
  }
  
  const BME680Readings& last() const { return _data; }
  
  bool initialized() const { return _initialized; }

private:
  Bsec2     _bsec;
  bool      _initialized = false;
  BME680Readings _data;
  
  // Callback statique → instance
  static void newDataCallback_static(const bme68xData data, 
                                     const bsecOutputs outputs, 
                                     Bsec2 bsec) {
    // Récupère l'instance singleton
    extern BME680Sensor& bme680_instance();
    bme680_instance()._onData(data, outputs, bsec);
  }
  
  void _onData(const bme68xData data, const bsecOutputs outputs, Bsec2 /*bsec*/) {
    for (uint8_t i = 0; i < outputs.nOutputs; i++) {
      const bsecData& out = outputs.output[i];
      switch (out.sensor_id) {
        case BSEC_OUTPUT_IAQ:
          _data.iaq = out.signal;
          _data.iaq_accuracy = out.accuracy;
          break;
        case BSEC_OUTPUT_STATIC_IAQ:
          _data.s_iaq = out.signal;
          break;
        case BSEC_OUTPUT_CO2_EQUIVALENT:
          _data.co2eq_ppm = out.signal;
          break;
        case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
          _data.bvoc_eq_ppm = out.signal;
          break;
        case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
          _data.temp_comp_c = out.signal;
          break;
        case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
          _data.hum_comp_pct = out.signal;
          break;
        case BSEC_OUTPUT_RAW_PRESSURE:
          _data.pressure_pa = (uint32_t)(out.signal * 100.0f + 0.5f);
          break;
        case BSEC_OUTPUT_RAW_GAS:
          _data.gas_ohms = (uint32_t)(out.signal + 0.5f);
          break;
        case BSEC_OUTPUT_GAS_PERCENTAGE:
          _data.gas_pct = out.signal;
          break;
        case BSEC_OUTPUT_RAW_TEMPERATURE:
          _data.raw_temp_c = out.signal;
          break;
        case BSEC_OUTPUT_RAW_HUMIDITY:
          _data.raw_hum_pct = out.signal;
          break;
        case BSEC_OUTPUT_STABILIZATION_STATUS:
          _data.stab_status = (uint8_t)out.signal;
          break;
        case BSEC_OUTPUT_RUN_IN_STATUS:
          _data.run_in_status = (uint8_t)out.signal;
          break;
        default: break;
      }
    }
    _data.valid = true;
  }
};

// Singleton — déclaré dans le .cpp
extern BME680Sensor g_bme680;

#endif // BME680_SENSOR_H
