/**
 * bme680_sensor.cpp — Singleton BME680 + BSEC2
 */

#include "bme680_sensor.h"

// Singleton global
BME680Sensor g_bme680;

// Raccourci pour le callback statique
BME680Sensor& bme680_instance() { return g_bme680; }
