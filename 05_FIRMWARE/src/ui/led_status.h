/**
 * led_status.h — Module LED RGB NOISYLESS v2.0.0
 * 
 * Brochage :  LED_R=GPIO12, LED_G=GPIO13, LED_B=GPIO14
 * Logique   :  actif bas (LOW = allumée, HIGH = éteinte)
 * 
 * Séquence de boot :
 *   1. rouge fixe       → boot en cours
 *   2. bleu fixe        → tentative WiFi
 *   3. bleu ×N flashs   → affiche le 3ᵉ chiffre de la version (v2.0.0 → 0 flash)
 *   4. vert fixe 1s     → connecté
 *   5. éteinte          → idle
 * 
 * Runtime :
 *   - WiFi perdu → rouge fixe
 *   - Publication MQTT réussie → flash vert 200ms
 * 
 * Usage :
 *   led_init();            // setup()
 *   led_set(LED_RED);      // loop() si WiFi perdu
 *   led_flash_green();     // après mqtt.publish() réussi
 *   led_boot_sequence();   // après WiFi connecté (bloque ~1.5s)
 */

#ifndef LED_STATUS_H
#define LED_STATUS_H

#include <Arduino.h>

// Pins (actif bas)
#define PIN_LED_R 12
#define PIN_LED_G 13
#define PIN_LED_B 14

// Couleurs
typedef enum {
  LED_OFF   = 0,
  LED_RED   = 1,
  LED_GREEN = 2,
  LED_BLUE  = 3,
  LED_YELLOW = 4,  // rouge+vert simulé (flash rapide)
  LED_PURPLE = 5   // rouge+bleu pour OTA
} led_color_t;

// Timings
#define LED_BLINK_MS      150   // durée flash version
#define LED_GREEN_HOLD_MS 1000  // vert après connexion
#define LED_PUB_FLASH_MS  200   // flash vert publication

/**
 * Initialise les pins LED en OUTPUT, éteintes.
 */
void led_init();

/**
 * Allume une couleur (éteint les autres avant).
 */
void led_set(led_color_t color);

/**
 * Éteint toutes les LED.
 */
void led_off();

/**
 * Affiche le 3ᵉ chiffre de la version via N flashs bleus.
 * Bloquant (~N × 2 × LED_BLINK_MS ms).
 * @param version  chaîne "2.0.0" → 0 flash
 */
void led_show_version(const char* version);

/**
 * Séquence complète après connexion WiFi :
 * blink version → vert 1s → off.
 * Bloquant (~2s max).
 */
void led_boot_sequence(const char* version);

/**
 * Flash vert 200ms pour signaler une publication MQTT réussie.
 * Non-bloquant (juste une impulsion).
 */
void led_flash_green();

/**
 * Gère les timings LED (vert boot 1s, flash publish 200ms).
 * À appeler régulièrement (~20ms) depuis une tâche FreeRTOS.
 */
void led_tick();

#endif // LED_STATUS_H
