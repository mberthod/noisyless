/**
 * led_status.cpp — Implémentation LED RGB NOISYLESS
 */

#include "led_status.h"

static led_color_t g_current = LED_OFF;
static unsigned long g_green_start = 0;
static unsigned long g_pub_flash_start = 0;
static bool g_boot_done = false;

void led_init() {
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  digitalWrite(PIN_LED_R, HIGH);
  digitalWrite(PIN_LED_G, HIGH);
  digitalWrite(PIN_LED_B, HIGH);
}

void led_off() {
  digitalWrite(PIN_LED_R, HIGH);
  digitalWrite(PIN_LED_G, HIGH);
  digitalWrite(PIN_LED_B, HIGH);
  g_current = LED_OFF;
}

void led_set(led_color_t color) {
  led_off();
  switch (color) {
    case LED_RED:   digitalWrite(PIN_LED_R, LOW); break;
    case LED_GREEN: digitalWrite(PIN_LED_G, LOW); break;
    case LED_BLUE:  digitalWrite(PIN_LED_B, LOW); break;
    case LED_YELLOW:
      digitalWrite(PIN_LED_R, LOW);
      digitalWrite(PIN_LED_G, LOW);
      break;
    case LED_PURPLE:
      digitalWrite(PIN_LED_R, LOW);
      digitalWrite(PIN_LED_B, LOW);
      break;
    default: break;
  }
  g_current = color;
}

void led_show_version(const char* version) {
  // Parse SemVer major.minor.patch
  int major = 0, minor = 0, patch = 0;
  int dots = 0;
  String sMaj, sMin, sPat;
  for (const char* p = version; *p; p++) {
    if (*p == '.') {
      dots++;
      continue;
    }
    if (dots == 0) sMaj += *p;
    else if (dots == 1) sMin += *p;
    else if (dots == 2) {
      if (*p >= '0' && *p <= '9') sPat += *p;
      else break;
    }
  }
  major = sMaj.toInt();
  minor = sMin.toInt();
  patch = sPat.toInt();

  // Logique selon version
  if (major == 0 && minor == 0) {
    // 0.0.x → flash en bleu x fois
    for (int i = 0; i < patch; i++) {
      led_set(LED_BLUE);
      delay(LED_BLINK_MS);
      led_off();
      delay(LED_BLINK_MS);
    }
  } else if (major == 0 && minor > 0) {
    // 0.x.y → flash en bleu minor*y fois (ou minor*10+y)
    int count = minor * 10 + patch;
    if (count > 30) count = 30; // sécurité
    for (int i = 0; i < count; i++) {
      led_set(LED_BLUE);
      delay(LED_BLINK_MS);
      led_off();
      delay(LED_BLINK_MS);
    }
  } else if (major >= 1) {
    // 1.x.y → flash en vert major fois
    for (int i = 0; i < major; i++) {
      led_set(LED_GREEN);
      delay(LED_BLINK_MS);
      led_off();
      delay(LED_BLINK_MS);
    }
  }
}

void led_boot_sequence(const char* version) {
  led_show_version(version);
  led_set(LED_GREEN);
  g_green_start = millis();
  g_boot_done = true;
}

void led_flash_green() {
  led_set(LED_GREEN);
  g_pub_flash_start = millis();
}

/**
 * Appelé à chaque loop(). Gère les timings :
 *  - vert 1s après boot → off
 *  - flash vert 200ms après publish → off (sauf si WiFi perdu)
 */
void led_tick() {
  unsigned long now = millis();
  
  // Vert boot : 1s puis off
  if (g_boot_done && g_green_start > 0 && now - g_green_start >= LED_GREEN_HOLD_MS) {
    g_green_start = 0;
    led_off();
  }
  
  // Flash publication : 200ms puis off
  if (g_pub_flash_start > 0 && now - g_pub_flash_start >= LED_PUB_FLASH_MS) {
    g_pub_flash_start = 0;
    led_off();
  }
}
