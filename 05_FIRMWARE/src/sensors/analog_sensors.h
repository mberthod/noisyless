/**
 * analog_sensors.h — Capteurs analogiques NOISYLESS v2.0.0
 * 
 * Lux ×2 :   ADC 12-bit sur GPIO1 (AIN1), GPIO2 (AIN2)
 * Micro ×2 : ADC 12-bit peak-to-peak sur GPIO3 (AIN3), GPIO4 (AIN4)
 * 
 * Usage :
 *   AnalogSensors sensors;
 *   sensors.init();
 *   sensors.read();                    // bloque 50ms
 *   uint16_t lux1 = sensors.lux1_raw;
 *   uint16_t mic1 = sensors.mic1_pp;
 */

#ifndef ANALOG_SENSORS_H
#define ANALOG_SENSORS_H

#include <Arduino.h>

#define PIN_LUX1_AIN 1
#define PIN_LUX2_AIN 2
#define PIN_MIC1     3
#define PIN_MIC2     4
#define ANALOG_WINDOW_MS 50  // durée échantillonnage micros

struct AnalogReadings {
  uint16_t lux1_raw = 0;
  uint16_t lux2_raw = 0;
  uint16_t mic1_pp  = 0;   // peak-to-peak
  uint16_t mic2_pp  = 0;
};

class AnalogSensors {
public:
  void init() {
    analogReadResolution(12);
  }
  
  const AnalogReadings& read() {
    auto t0 = millis();
    
    _data.lux1_raw = analogRead(PIN_LUX1_AIN);
    _data.lux2_raw = analogRead(PIN_LUX2_AIN);
    
    // Échantillonnage micros sur 50ms pour calculer le peak-to-peak
    uint16_t min1 = 4095, max1 = 0;
    uint16_t min2 = 4095, max2 = 0;
    
    while (millis() - t0 < ANALOG_WINDOW_MS) {
      uint16_t s1 = analogRead(PIN_MIC1);
      uint16_t s2 = analogRead(PIN_MIC2);
      if (s1 < min1) min1 = s1;
      if (s1 > max1) max1 = s1;
      if (s2 < min2) min2 = s2;
      if (s2 > max2) max2 = s2;
      delay(1);
    }
    
    _data.mic1_pp = (max1 > min1) ? max1 - min1 : 0;
    _data.mic2_pp = (max2 > min2) ? max2 - min2 : 0;
    
    return _data;
  }
  
  const AnalogReadings& last() const { return _data; }

private:
  AnalogReadings _data;
};

#endif // ANALOG_SENSORS_H
