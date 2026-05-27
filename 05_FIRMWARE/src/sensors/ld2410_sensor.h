/**
 * ld2410_sensor.h — Radar LD2410 NOISYLESS v2.0.0
 * 
 * UART      : Serial1, 256000 bauds
 * Brochage  : LD2410 TX → ESP32 RX=GPIO18
 *             LD2410 RX → ESP32 TX=GPIO17
 * 
 * Usage :
 *   LD2410Sensor radar;
 *   radar.init();
 *   radar.tick();             // lit les trames (depuis task 100ms)
 *   bool pres = radar.presence();
 *   uint16_t dist = radar.distance_cm();
 */

#ifndef LD2410_SENSOR_H
#define LD2410_SENSOR_H

#include <Arduino.h>
#include <ld2410.h>

// Pins UART
#define PIN_LD2410_RX  18  // ESP RX ← LD2410 TX
#define PIN_LD2410_TX  17  // ESP TX → LD2410 RX

class LD2410Sensor {
public:
  bool init() {
    _serial.begin(256000, SERIAL_8N1, PIN_LD2410_RX, PIN_LD2410_TX);
    delay(100);
    
    if (!_radar.begin(_serial)) {
      Serial.println("[LD2410] Introuvable / non initialisé");
      return false;
    }
    
    Serial.println("[LD2410] Initialisé (256000 bauds)");
    _ok = true;
    return true;
  }
  
  /**
   * Lit les trames LD2410. À appeler régulièrement (task 100ms).
   */
  void tick() {
    if (!_ok) return;
    _radar.read();
    
    bool motion = _radar.movingTargetDetected();
    bool still  = _radar.stationaryTargetDetected();
    _presence = motion || still;
    
    if (_presence) {
      uint16_t dMove = _radar.movingTargetDistance();
      uint16_t dStill = _radar.stationaryTargetDistance();
      _distance_cm = (dMove > 0) ? dMove : dStill;
    } else {
      _distance_cm = 0;
    }
  }
  
  bool     presence()    const { return _presence; }
  uint16_t distance_cm() const { return _distance_cm; }
  bool     ok()          const { return _ok; }

private:
  ld2410          _radar;
  HardwareSerial  _serial = Serial1;
  bool            _ok = false;
  bool            _presence = false;
  uint16_t        _distance_cm = 0;
};

#endif // LD2410_SENSOR_H
