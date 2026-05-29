/**
 * @file main.cpp
 * @brief Firmware Noisyless pour ESP32-S3 (8 Mo flash)
 *
 * Architecture monolithique avec :
 * - WiFiManager (portail captif) + MQTT (PubSubClient)
 * - BME680 / BSEC2 (qualité de l'air intérieur)
 * - LD2410 (radar présence/mouvement UART)
 * - Capteurs analogiques (luxmètres, microphones)
 * - Mise à jour OTA via manifest JSON + SHA-256
 *
 * @version 0.0.3
 * @author Noisyless
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <bsec2.h>    // Préférer l'API Bsec (compatible BSEC2 Arduino)
#include <HardwareSerial.h>
#include <ld2410.h>  // Bibliothèque LD2410 (ncmreynolds/ld2410@^0.1.4)
#include <HTTPClient.h>
#include <Update.h>
#include <mbedtls/sha256.h>
#include <Preferences.h>

#include "credentials.h"  // Contient uniquement les secrets MQTT
#include "ui/led_status.h"

/** @name Brochage (Pinout)
 *  Définitions des broches GPIO de l'ESP32-S3 pour chaque capteur et actionneur */
/** @{ */
static const int PIN_LUX1_AIN = 1;         /**< Entrée analogique luxmètre 1 */
static const int PIN_LUX2_AIN = 2;         /**< Entrée analogique luxmètre 2 */
static const int PIN_MIC1 = 3;             /**< Entrée analogique microphone 1 */
static const int PIN_MIC2 = 4;             /**< Entrée analogique microphone 2 */
static const int PIN_SYSTEM_POWERED = 5;   /**< Indicateur d'alimentation système */
static const int PIN_SW_OPEN = 11;         /**< Interrupteur d'ouverture boîtier */
// PIN_LED_R, PIN_LED_G, PIN_LED_B sont définies dans ui/led_status.h
static const int PIN_LD2410_RX_ESP = 18;   /**< LD2410 TX → ESP RX (UART) */
static const int PIN_LD2410_TX_ESP = 17;   /**< LD2410 RX → ESP TX (UART) */
static const int PIN_OUT_SIGNAL = 21;      /**< Sortie signal générale */
static const int PIN_BME_INT = 34;         /**< Interruption BME680 (utilisé comme sélecteur d'adresse I2C) */
static const int PIN_I2C_SDA = 35;         /**< Bus I2C SDA */
static const int PIN_I2C_SCL = 36;         /**< Bus I2C SCL */
static const int PIN_BME_MODE = 37;        /**< Sélecteur mode BME680 (LOW = 0x76, HIGH = 0x77) */
/** @} */

/** @name Configuration Produit
 *  Identifiants, version et credentials WiFi de secours */
/** @{ */
static const char* WIFI_SSID = "TP-Link_B150";   /**< SSID WiFi de secours (fallback) */
static const char* WIFI_PASS = "12902637";        /**< Mot de passe WiFi de secours */

/** WiFiManager — portail captif pour configuration client */
static WiFiManager wifiManager;                   /**< Instance du gestionnaire WiFi avec portail captif */
static Preferences prefs;                         /**< Stockage persistant NVS */
static const char* PREFS_NS = "noisyless";        /**< Namespace NVS pour les credentials WiFi */

static const char* PRODUCT = "noisyless_env";     /**< Identifiant produit pour les messages MQTT */
static const char* CLIENT_CODE = "client_demo";   /**< Code client pour les messages MQTT */
static char DEVICE_ID[20] = {0};                  /**< Identifiant unique : NL-XXXXXXXXXXXX (basé sur MAC, généré au boot) */
static char CLIENT_ID[32] = {0};                  /**< Identifiant MQTT : noisyless_<device_id> */
static const char* FW_VERSION = "0.0.7";          /**< Version actuelle du firmware (SemVer) */
/** @} */

/** @name Constantes temporelles
 *  Intervalles de publication, d'échantillonnage et de vérification OTA */
/** @{ */
static const unsigned long PUBLISH_INTERVAL_MS = 10000UL;   /**< Intervalle de publication MQTT : 10 secondes */
static const unsigned long SENSOR_ADC_WINDOW_MS = 50UL;     /**< Fenêtre d'échantillonnage ADC (microphones) : 50 ms */
static const unsigned long RADAR_READ_PERIOD_MS = 100UL;    /**< Période de lecture du radar LD2410 : 100 ms */
static const unsigned long OTA_CHECK_INTERVAL_MS = 60000UL; /**< Intervalle de vérification OTA : 1 minute */
/** @} */

/** @name Configuration OTA
 *  Manifeste et paramètres de mise à jour à distance */
/** @{ */
static const char* OTA_MANIFEST_URL = "https://noisyless.com/firmware/stable.json"; /**< URL du manifeste OTA (canal stable) */
static const char* OTA_USER_AGENT = "Noisyless-ESP32"; /**< User-Agent pour les requêtes HTTP OTA */
/** @} */

/** @name Objets globaux
 *  Instances des bibliothèques et objets partagés entre les tâches */
/** @{ */
WiFiClient espClient;          /**< Client WiFi pour la connexion MQTT */
PubSubClient mqtt(espClient);  /**< Client MQTT basé sur espClient */

ld2410 radar;                   /**< Capteur LD2410 (radar présence/mouvement) */
HardwareSerial& RadarSerial = Serial1; /**< Port série UART dédié au radar */
/** @} */


#define ERROR_DUR 1000   /**< Durée d'affichage de l'erreur LED (ms), non utilisée directement */

#define SAMPLE_RATE BSEC_SAMPLE_RATE_ULP /**< Taux d'échantillonnage BSEC2 : Ultra Low Power */


/** @name Indicateurs et variables globales
 *  Drapeaux d'état, tampons et mutex pour la synchronisation inter-tâches */
/** @{ */
static bool g_bsec_ok = false;                    /**< Drapeau : initialisation BSEC2 réussie */
static bool g_ld_ok = false;                      /**< Drapeau : initialisation LD2410 réussie */
static unsigned long g_lastBsecInitAttemptMs = 0; /**< Dernière tentative d'initialisation BSEC2 (ms) */
static unsigned long g_lastPublishMs = 0;         /**< Dernière publication MQTT (ms) */
static char g_mqttTopic[128] = {0};               /**< Topic MQTT pour les données JSON */
static char g_mqttTopicLine[128] = {0};           /**< Topic MQTT pour le format ligne (InfluxDB) */
static char g_macStr[20] = {0};                   /**< Chaîne MAC (12 caractères hexadécimaux) */
void newDataCallback(const bme68xData data, const bsecOutputs outputs, Bsec2 bsec); /**< Forward declaration du callback BSEC */
/** @} */

/* Create an object of the class Bsec2 */
Bsec2 envSensor; /**< Instance du capteur environnemental BME680 via l'API BSEC2 */

/**
 * @brief Dernières valeurs BSEC pour alimentation du JSON
 *
 * Structure volatile contenant toutes les sorties du capteur BME680
 * traitées par l'algorithme BSEC2. Mise à jour par le callback
 * newDataCallback() et lue par publishMeasurements().
 */
struct BsecLastValues {
  bool valid = false;           /**< Drapeau : les données BSEC sont valides et à jour */
  float temp_comp_c = 0.0f;    /**< Température compensée (°C) */
  float hum_comp_pct = 0.0f;   /**< Humidité relative compensée (%) */
  uint32_t pressure_pa = 0;    /**< Pression atmosphérique (Pa) */
  uint32_t gas_ohms = 0;       /**< Résistance du gaz capteur compensée (Ω) */
  float raw_temp_c = 0.0f;     /**< Température brute avant compensation (°C) */
  float raw_hum_pct = 0.0f;    /**< Humidité brute avant compensation (%) */
  uint32_t raw_gas_ohms = 0;   /**< Résistance du gaz brute (Ω) */
  float iaq = 0.0f;            /**< Indice de Qualité de l'Air Intérieur (IAQ) */
  float s_iaq = 0.0f;          /**< Indice IAQ statique (s_IAQ) */
  float co2eq_ppm = 0.0f;      /**< Concentration CO2 estimée (ppm) */
  float bvoc_eq_ppm = 0.0f;    /**< Concentration équivalente COV respiratoires (ppm) */
  float gas_pct = 0.0f;        /**< Pourcentage de gaz (%) */
  uint8_t iaq_accuracy = 0;    /**< Précision de l'IAQ (0 = non stabilisé, 3 = haute précision) */
  uint8_t stab_status = 0;     /**< Statut de stabilisation */
  uint8_t run_in_status = 0;   /**< Statut de rodage (run-in) */
};
static volatile BsecLastValues g_bsec_last; /**< Dernières valeurs BSEC, volatiles car modifiées depuis le callback */

/**
 * @brief Données partagées entre tâches (protégées par mutex)
 *
 * Structure contenant l'état complet des capteurs, lue par la tâche
 * MQTT pour la publication JSON. La synchronisation est assurée par
 * le sémaphore g_sharedMutex.
 */
struct SharedMeasurements {
  BsecLastValues bsec;      /**< Mesures environnementales BSEC2 */
  bool presence = false;    /**< Présence détectée par le radar LD2410 */
  uint16_t distance_cm = 0; /**< Distance de la cible détectée (cm) */
  uint16_t lux1_raw = 0;    /**< Valeur brute luxmètre 1 (ADC 0-4095) */
  uint16_t lux2_raw = 0;    /**< Valeur brute luxmètre 2 (ADC 0-4095) */
  uint16_t mic1_pp = 0;     /**< Amplitude pic-à-pic microphone 1 (ADC) */
  uint16_t mic2_pp = 0;     /**< Amplitude pic-à-pic microphone 2 (ADC) */
};
static SharedMeasurements g_shared{};                     /**< Instance globale des mesures partagées */
static SemaphoreHandle_t g_sharedMutex = nullptr;         /**< Mutex protégeant l'accès aux mesures partagées */
static SemaphoreHandle_t g_otaMutex = nullptr;            /**< Mutex protégeant les opérations OTA */
static bool g_otaRunning = false;                         /**< Drapeau : une mise à jour OTA est en cours */
/** @} */


/** @name Fonctions utilitaires
 *  Fonctions de traduction d'erreurs et d'indication visuelle */
/** @{ */

/**
 * @brief Traduit un code d'erreur MQTT en chaîne lisible
 *
 * Convertit le code retourné par PubSubClient::state() en une chaîne
 * descriptive pour le débogage série.
 *
 * @param code Code d'état MQTT (de -4 à 5)
 * @return const char* Chaîne descriptive du code d'erreur
 */
static const char* mqttError(int8_t code) {
  switch (code) {
    case -4: return "MQTT_CONNECTION_TIMEOUT";
    case -3: return "MQTT_CONNECTION_LOST";
    case -2: return "MQTT_CONNECT_FAILED";
    case -1: return "MQTT_DISCONNECTED";
    case  0: return "MQTT_CONNECTED";
    case  1: return "MQTT_CONNECT_BAD_PROTOCOL";
    case  2: return "MQTT_CONNECT_BAD_CLIENT_ID";
    case  3: return "MQTT_CONNECT_UNAVAILABLE";
    case  4: return "MQTT_CONNECT_BAD_CREDENTIALS";
    case  5: return "MQTT_CONNECT_NOT_AUTHORIZED";
  }
  return "UNKNOWN";
}

/**
 * @brief Clignotement LED rouge pour signaler une erreur
 *
 * Allume la LED rouge pendant 100 ms puis l'éteint. Utilisé par
 * checkBsecStatus() pour signaler les erreurs BSEC/BME68X.
 */
void errLeds(void)
{
  digitalWrite(PIN_LED_R, HIGH);
  delay(100);
  digitalWrite(PIN_LED_R, LOW);
  delay(100);
}
/** @} */


/** @name Fonctions BSEC2
 *  Vérification d'état et callback de réception des données BSEC */
/** @{ */

/**
 * @brief Vérifie et affiche les codes d'état/erreur BSEC2 et BME68X
 *
 * Analyse les champs status de l'objet Bsec2 et de son capteur BME68X.
 * En cas d'erreur, appelle errLeds() pour le signaler visuellement.
 * Les avertissements sont simplement logués sur la console série.
 *
 * @param bsec Référence à l'objet Bsec2 dont on vérifie le statut
 */
void checkBsecStatus(Bsec2 bsec)
{
  if (bsec.status < BSEC_OK)
  {
    Serial.println("BSEC error code : " + String(bsec.status));
    errLeds(); /* Blink LEDs once, then continue */
  }
  else if (bsec.status > BSEC_OK)
  {
    Serial.println("BSEC warning code : " + String(bsec.status));
  }

  if (bsec.sensor.status < BME68X_OK)
  {
    Serial.println("BME68X error code : " + String(bsec.sensor.status));
    errLeds(); /* Blink LEDs once, then continue */
  }
  else if (bsec.sensor.status > BME68X_OK)
  {
    Serial.println("BME68X warning code : " + String(bsec.sensor.status));
  }
}

/**
 * @brief Callback de réception des données BSEC2
 *
 * Invoqué automatiquement par l'API BSEC2 à chaque nouveau cycle
 * d'acquisition. Parse l'ensemble des sorties configurées (IAQ,
 * température, humidité, pression, gaz, etc.) et remplit la
 * structure globale g_bsec_last.
 *
 * @param data Données brutes du capteur BME68X
 * @param outputs Sorties traitées par l'algorithme BSEC
 * @param bsec Référence à l'objet Bsec2 appelant
 */
void newDataCallback(const bme68xData data, const bsecOutputs outputs, Bsec2 bsec)
{
  if (!outputs.nOutputs)
  {
    return;
  }

  Serial.println("BSEC outputs:\n\tTime stamp = " + String((int)(outputs.output[0].time_stamp / INT64_C(1000000))));
  for (uint8_t i = 0; i < outputs.nOutputs; i++)
  {
    const bsecData output = outputs.output[i];
    switch (output.sensor_id)
    {
    case BSEC_OUTPUT_IAQ:
      Serial.println("\tIAQ = " + String(output.signal));
      Serial.println("\tIAQ accuracy = " + String((int)output.accuracy));
      g_bsec_last.iaq = output.signal;
      g_bsec_last.iaq_accuracy = output.accuracy;
      break;
    case BSEC_OUTPUT_RAW_TEMPERATURE:
      Serial.println("\tTemperature = " + String(output.signal));
      g_bsec_last.raw_temp_c = output.signal;
      break;
    case BSEC_OUTPUT_RAW_PRESSURE:
      Serial.println("\tPressure = " + String(output.signal));
      // La lib affiche en hPa dans l'exemple → convertir en Pa pour le JSON
      g_bsec_last.pressure_pa = (uint32_t)(output.signal * 100.0f + 0.5f);
      break;
    case BSEC_OUTPUT_RAW_HUMIDITY:
      Serial.println("\tHumidity = " + String(output.signal));
      g_bsec_last.raw_hum_pct = output.signal;
      break;
    case BSEC_OUTPUT_RAW_GAS:
      Serial.println("\tGas resistance = " + String(output.signal));
      g_bsec_last.raw_gas_ohms = (uint32_t)(output.signal + 0.5f);
      g_bsec_last.gas_ohms = (uint32_t)(output.signal + 0.5f);
      break;
    case BSEC_OUTPUT_STABILIZATION_STATUS:
      Serial.println("\tStabilization status = " + String(output.signal));
      g_bsec_last.stab_status = (uint8_t)output.signal;
      break;
    case BSEC_OUTPUT_RUN_IN_STATUS:
      Serial.println("\tRun in status = " + String(output.signal));
      g_bsec_last.run_in_status = (uint8_t)output.signal;
      break;
    case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
      Serial.println("\tCompensated temperature = " + String(output.signal));
      g_bsec_last.temp_comp_c = output.signal;
      break;
    case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
      Serial.println("\tCompensated humidity = " + String(output.signal));
      g_bsec_last.hum_comp_pct = output.signal;
      break;
    case BSEC_OUTPUT_STATIC_IAQ:
      Serial.println("\tStatic IAQ = " + String(output.signal));
      g_bsec_last.s_iaq = output.signal;
      break;
    case BSEC_OUTPUT_CO2_EQUIVALENT:
      Serial.println("\tCO2 Equivalent = " + String(output.signal));
      g_bsec_last.co2eq_ppm = output.signal;
      break;
    case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
      Serial.println("\tbVOC equivalent = " + String(output.signal));
      g_bsec_last.bvoc_eq_ppm = output.signal;
      break;
    case BSEC_OUTPUT_GAS_PERCENTAGE:
      Serial.println("\tGas percentage = " + String(output.signal));
      g_bsec_last.gas_pct = output.signal;
      break;
    case BSEC_OUTPUT_COMPENSATED_GAS:
      Serial.println("\tCompensated gas = " + String(output.signal));
      break;
    default:
      break;
    }
  }
  g_bsec_last.valid = true;
}
/** @} */


/** @name Initialisation BSEC2
 *  Configuration du capteur BME680 via l'API BSEC2 */
/** @{ */

/**
 * @brief Initialise le capteur BME680 via l'API BSEC2
 *
 * Configure le bus I2C, pilote le sélecteur d'adresse (PIN_BME_MODE),
 * tente l'initialisation du capteur à l'adresse 0x76, abonne les
 * sorties BSEC2 souhaitées et attache le callback de réception des
 * données. Fonctionne en mode Ultra Low Power (ULP).
 *
 * @return true si l'initialisation a réussi, false en cas d'échec
 *         (capteur absent ou erreur I2C)
 */
bool initBSEC() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  // Pilote le sélecteur d'adresse (LOW=0x76, HIGH=0x77)
  // On essaie d'abord LOW (0x76), puis HIGH (0x77) si le scan I2C échoue
  pinMode(PIN_BME_INT, OUTPUT);
  digitalWrite(PIN_BME_INT, HIGH);

  pinMode(PIN_BME_MODE, OUTPUT);
  digitalWrite(PIN_BME_MODE, LOW);
  delay(5);

  // Déterminer l'adresse I2C du BME688 par scan matériel
  uint8_t bmeAddr = 0;
  const uint8_t addrCandidates[2] = {BME68X_I2C_ADDR_LOW, BME68X_I2C_ADDR_HIGH};
  const int modePins[2] = {LOW, HIGH};

  for (int attempt = 0; attempt < 2; attempt++) {
    // Piloter la broche de sélection d'adresse
    digitalWrite(PIN_BME_MODE, modePins[attempt]);
    delay(5);

    // Scanner l'adresse candidate sur le bus I2C
    Wire.beginTransmission(addrCandidates[attempt]);
    if (Wire.endTransmission() == 0) {
      bmeAddr = addrCandidates[attempt];
      Serial.printf("[BSEC2] BME688 détecté à 0x%02X (PIN_BME_MODE=%s)\n",
                    bmeAddr, modePins[attempt] == HIGH ? "HIGH" : "LOW");
      break;
    }
    Serial.printf("[BSEC2] Aucun capteur à 0x%02X (PIN_BME_MODE=%s), tentative suivante...\n",
                  addrCandidates[attempt], modePins[attempt] == HIGH ? "HIGH" : "LOW");
  }

  if (bmeAddr == 0) {
    Serial.println("[BSEC2] Aucun capteur BME688 trouvé sur le bus I2C — mode dégradé");
    return false;
  }

  bsecSensor sensorList[] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_STABILIZATION_STATUS,
    BSEC_OUTPUT_RUN_IN_STATUS,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_GAS_PERCENTAGE,
    BSEC_OUTPUT_COMPENSATED_GAS};

  /* Initialize the library and interfaces */
  if (!envSensor.begin(bmeAddr, Wire))
  {
    checkBsecStatus(envSensor);
    Serial.println("[BSEC2] Échec begin() — capteur BME680 absent ou I2C error, utilisation mode dégradé");
    return false;  // ← RETURN DIRECT si pas de capteur
  }

  /*
   *	The default offset provided has been determined by testing the sensor in LP and ULP mode on application board 3.0
   *	Please update the offset value after testing this on your product
   */
  if (SAMPLE_RATE == BSEC_SAMPLE_RATE_ULP)
  {
    envSensor.setTemperatureOffset(TEMP_OFFSET_ULP);
  }
  else if (SAMPLE_RATE == BSEC_SAMPLE_RATE_LP)
  {
    envSensor.setTemperatureOffset(TEMP_OFFSET_LP);
  }

  /* Subsribe to the desired BSEC2 outputs */
  if (!envSensor.updateSubscription(sensorList, ARRAY_LEN(sensorList), SAMPLE_RATE))
  {
    checkBsecStatus(envSensor);
    Serial.println("[BSEC2] Échec updateSubscription — mode dégradé");
    return false;
  }

  /* Whenever new data is available call the newDataCallback function */
  envSensor.attachCallback(newDataCallback);

  Serial.println("BSEC library version " +
                 String(envSensor.version.major) + "." + String(envSensor.version.minor) + "." + String(envSensor.version.major_bugfix) + "." + String(envSensor.version.minor_bugfix));
  return true;
}
/** @} */


/** @name Initialisation LD2410
 *  Configuration du radar UART LD2410 */
/** @{ */

/**
 * @brief Initialise la communication UART avec le capteur LD2410
 *
 * Configure le port série matériel Serial1 à 256000 bauds pour
 * communiquer avec le radar LD2410. Utilise un parseur de trames
 * personnalisé (pas la bibliothèque ld2410 standard).
 *
 * @return true toujours (le parseur fonctionne indépendamment de l'état du capteur)
 */
bool initLD2410() {
  static bool serial_started = false;
  if (!serial_started) {
    RadarSerial.begin(256000, SERIAL_8N1, PIN_LD2410_RX_ESP, PIN_LD2410_TX_ESP);
    serial_started = true;
    Serial.println("[LD2410] UART @256000 baud, parser custom");
  }
  return true; // Toujours OK, on parse directement les frames
}
/** @} */


/** @name Parseur LD2410
 *  Analyse des trames radar en mode cyclique */
/** @{ */

/**
 * @brief Structure interne des données parsées du LD2410
 *
 * Frame target data (mode cyclique) :
 *   F4 F3 F2 F1 | LL LL | 02 AA | STATE | MD_L MD_H | ME | SD_L SD_H | SE | DD_L DD_H | 55 | CK | F8 F7 F6 F5
 * STATE : 0 = aucune présence, 1 = mouvement, 2 = stationnaire, 3 = les deux
 */
static struct {
  bool valid;                     /**< Drapeau : dernière trame valide reçue */
  uint8_t state;                  /**< État de présence (0-3 : none/moving/stationary/both) */
  uint16_t moving_dist_cm;       /**< Distance de la cible en mouvement (cm) */
  uint8_t moving_energy;          /**< Énergie de la cible en mouvement */
  uint16_t still_dist_cm;        /**< Distance de la cible stationnaire (cm) */
  uint8_t still_energy;           /**< Énergie de la cible stationnaire */
  uint16_t detection_dist_cm;    /**< Distance de détection maximale (cm) */
  unsigned long last_update_ms;   /**< Horodatage de la dernière mise à jour (ms) */
} g_ld_data = {};

/**
 * @brief Parse le flux UART du radar LD2410 en temps réel
 *
 * Analyse les trames binaires au format propriétaire LD2410
 * (délimiteurs F4F3F2F1 / F8F7F6F5). Extrait l'état de présence,
 * les distances et énergies des cibles. Fonctionne en mode non
 * bloquant (appelée à chaque itération de la tâche radar).
 */
void parseLD2410Stream() {
  static uint8_t buf[64];
  static size_t pos = 0;
  static const uint8_t HDR[4] = {0xF4, 0xF3, 0xF2, 0xF1};

  while (RadarSerial.available()) {
    uint8_t b = RadarSerial.read();
    if (pos < sizeof(buf)) buf[pos++] = b;

    // Cherche le header
    if (pos < 4) continue;
    if (memcmp(buf, HDR, 4) != 0) {
      // shift left
      memmove(buf, buf + 1, pos - 1);
      pos--;
      continue;
    }
    // On a le header. Attendre au moins 6 bytes pour lire la longueur.
    if (pos < 6) continue;
    uint16_t len = buf[4] | (buf[5] << 8);
    size_t total = 4 + 2 + len + 4; // header + len + payload + footer
    if (total > sizeof(buf)) { pos = 0; continue; } // trop grand, reset
    if (pos < total) continue;

    // Verifier footer
    if (buf[total-4] == 0xF8 && buf[total-3] == 0xF7 &&
        buf[total-2] == 0xF6 && buf[total-1] == 0xF5) {
      // Frame complete - parser type 02 AA (target data)
      if (len >= 11 && buf[6] == 0x02 && buf[7] == 0xAA) {
        g_ld_data.state             = buf[8];
        g_ld_data.moving_dist_cm    = buf[9]  | (buf[10] << 8);
        g_ld_data.moving_energy     = buf[11];
        g_ld_data.still_dist_cm     = buf[12] | (buf[13] << 8);
        g_ld_data.still_energy      = buf[14];
        g_ld_data.detection_dist_cm = buf[15] | (buf[16] << 8);
        g_ld_data.valid = true;
        g_ld_data.last_update_ms = millis();
      }
    }
    // Avancer apres ce frame (qu'il soit valide ou non)
    if (pos > total) memmove(buf, buf + total, pos - total);
    pos = (pos > total) ? pos - total : 0;
  }
}
/** @} */


/** @name Fonctions de lecture des capteurs
 *  Radar, luminosité et microphones */
/** @{ */

/**
 * @brief Lit l'état de présence et la distance depuis le radar LD2410
 *
 * Appelle parseLD2410Stream() puis analyse les données parsées.
 * Si aucune trame reçue depuis plus de 2 secondes, le capteur est
 * considéré hors ligne. La distance retournée priorise la cible en
 * mouvement, puis la cible stationnaire, puis la détection maximale.
 *
 * @param[out] presence Référence booléenne : true si présence détectée
 * @param[out] distance_cm Référence distance de la cible en cm
 * @return true si les données radar sont valides, false si offline
 */
bool readLD2410(bool& presence, uint16_t& distance_cm) {
  parseLD2410Stream();

  // Si pas de frame recu depuis 2s, considere offline
  if (!g_ld_data.valid || (millis() - g_ld_data.last_update_ms > 2000)) {
    presence = false;
    distance_cm = 0;
    return false;
  }

  bool motion = (g_ld_data.state & 0x01) != 0;
  bool still  = (g_ld_data.state & 0x02) != 0;
  presence = motion || still;

  if (motion && g_ld_data.moving_dist_cm > 0) {
    distance_cm = g_ld_data.moving_dist_cm;
  } else if (still && g_ld_data.still_dist_cm > 0) {
    distance_cm = g_ld_data.still_dist_cm;
  } else {
    distance_cm = g_ld_data.detection_dist_cm;
  }
  return true;
}

/**
 * @brief Lit les deux entrées analogiques de luminosité
 *
 * Effectue une lecture analogique simple sur les deux canaux
 * luxmètres. Les valeurs brutes (0-4095 pour ESP32-S3 sur 12 bits)
 * sont retournées par référence.
 *
 * @param[out] lux1_raw Valeur ADC brute du luxmètre 1
 * @param[out] lux2_raw Valeur ADC brute du luxmètre 2
 * @return true toujours
 */
bool readLightSensors(uint16_t& lux1_raw, uint16_t& lux2_raw) {
  lux1_raw = analogRead(PIN_LUX1_AIN);
  lux2_raw = analogRead(PIN_LUX2_AIN);
  return true;
}

/**
 * @brief Mesure le niveau sonore par amplitude pic-à-pic
 *
 * Échantillonne les deux microphones analogiques pendant une fenêtre
 * de ~50 ms et calcule l'amplitude crête-à-crête (max - min) pour
 * chaque canal comme indicateur du niveau sonore.
 *
 * @param[out] mic1_pp Amplitude pic-à-pic du microphone 1
 * @param[out] mic2_pp Amplitude pic-à-pic du microphone 2
 * @return true toujours
 */
bool readMicrophones(uint16_t& mic1_pp, uint16_t& mic2_pp) {
  const uint32_t windowMs = 50;
  uint32_t t0 = millis();

  uint16_t min1 = 4095, max1 = 0;
  uint16_t min2 = 4095, max2 = 0;

  while ((millis() - t0) < windowMs) {
    uint16_t s1 = analogRead(PIN_MIC1);
    uint16_t s2 = analogRead(PIN_MIC2);
    if (s1 < min1) min1 = s1;
    if (s1 > max1) max1 = s1;
    if (s2 < min2) min2 = s2;
    if (s2 > max2) max2 = s2;
    delay(1);
  }

  mic1_pp = (max1 > min1) ? (max1 - min1) : 0;
  mic2_pp = (max2 > min2) ? (max2 - min2) : 0;
  return true;
}
/** @} */


/** @name Publieur MQTT
 *  Construction et envoi des données au format JSON */
/** @{ */

/**
 * @brief Construit et publie le JSON de mesures sur MQTT
 *
 * Rassemble les données BSEC2 (température, humidité, pression, IAQ,
 * CO2, COV, gaz), radar (présence, distance), ADC (luxmètres,
 * microphones) et système (RSSI, uptime, timestamp) dans un unique
 * payload JSON. Publie sur le topic MQTT dédié.
 *
 * Le format JSON est conçu pour Telegraf (data_format = "json").
 * Les données partagées sont lues sous protection du mutex
 * g_sharedMutex.
 */
void publishMeasurements() {
  float temperature_c = 0.0f;
  float humidity_pct = 0.0f;
  uint32_t pressure_pa = 0;
  uint32_t gas_ohms = 0;

  float raw_temperature_c = 0.0f;
  float raw_humidity_pct = 0.0f;
  uint32_t raw_gas_ohms = 0;

  float iaq = 0.0f;
  float s_iaq = 0.0f;
  float co2eq_ppm = 0.0f;
  float bvoc_eq_ppm = 0.0f;
  float gas_pct = 0.0f;
  uint8_t iaq_accuracy = 0;
  uint8_t stab_status = 0;
  uint8_t run_in_status = 0;

  // --- Données BSEC si valides ---
  if (g_bsec_ok && g_bsec_last.valid) {
    temperature_c    = g_bsec_last.temp_comp_c;
    humidity_pct     = g_bsec_last.hum_comp_pct;
    pressure_pa      = g_bsec_last.pressure_pa;
    gas_ohms         = g_bsec_last.gas_ohms;

    raw_temperature_c = g_bsec_last.raw_temp_c;
    raw_humidity_pct  = g_bsec_last.raw_hum_pct;
    raw_gas_ohms      = g_bsec_last.raw_gas_ohms;

    iaq           = g_bsec_last.iaq;
    s_iaq         = g_bsec_last.s_iaq;
    co2eq_ppm     = g_bsec_last.co2eq_ppm;
    bvoc_eq_ppm   = g_bsec_last.bvoc_eq_ppm;
    gas_pct       = g_bsec_last.gas_pct;
    iaq_accuracy  = g_bsec_last.iaq_accuracy;
    stab_status   = g_bsec_last.stab_status;
    run_in_status = g_bsec_last.run_in_status;
  }

  // --- Radar + ADC depuis le buffer partagé ---
  bool presence = false;
  uint16_t target_distance_cm = 0;
  uint16_t lux1_raw = 0, lux2_raw = 0;
  uint16_t mic1_pp = 0, mic2_pp = 0;
  if (g_sharedMutex && xSemaphoreTake(g_sharedMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    presence = g_shared.presence;
    target_distance_cm = g_shared.distance_cm;
    lux1_raw = g_shared.lux1_raw;
    lux2_raw = g_shared.lux2_raw;
    mic1_pp = g_shared.mic1_pp;
    mic2_pp = g_shared.mic2_pp;
    xSemaphoreGive(g_sharedMutex);
  }
  uint8_t people_count = presence ? 1 : 0;

  // --- Système ---
  int rssi_dbm = WiFi.RSSI();
  unsigned long uptime_s = millis() / 1000UL;
  unsigned long timestamp_ms = millis();

  // --- JSON unique pour Telegraf (data_format = "json") ---
  char payload[1024];
  int n = snprintf(
    payload, sizeof(payload),
    "{"
      "\"product\":\"%s\","
      "\"client_code\":\"%s\","
      "\"device_id\":\"%s\","
      "\"fw_version\":\"%s\","
      "\"temperature_c\":%.2f,"
      "\"humidity_pct\":%.2f,"
      "\"pressure_pa\":%u,"
      "\"gas_ohms\":%u,"
      "\"iaq\":%.2f,"
      "\"s_iaq\":%.2f,"
      "\"co2eq_ppm\":%.2f,"
      "\"bvoc_eq_ppm\":%.2f,"
      "\"iaq_accuracy\":%u,"
      "\"stab_status\":%u,"
      "\"run_in_status\":%u,"
      "\"gas_pct\":%.2f,"
      "\"raw_temperature_c\":%.2f,"
      "\"raw_humidity_pct\":%.2f,"
      "\"raw_gas_ohms\":%u,"
      "\"sensor_comp_temperature_c\":%.2f,"
      "\"sensor_comp_humidity_pct\":%.2f,"
      "\"sensor_comp_gas_ohms\":%u,"
      "\"lux1_raw\":%u,"
      "\"lux2_raw\":%u,"
      "\"sound_mic1_pp\":%u,"
      "\"sound_mic2_pp\":%u,"
      "\"presence\":%s,"
      "\"people_count\":%u,"
      "\"target_distance_cm\":%u,"
      "\"rssi_dbm\":%d,"
      "\"uptime_s\":%lu,"
      "\"timestamp_ms\":%lu"
    "}",
    PRODUCT,
    CLIENT_CODE,
    DEVICE_ID,
    FW_VERSION,
    temperature_c,
    humidity_pct,
    pressure_pa,
    gas_ohms,
    iaq,
    s_iaq,
    co2eq_ppm,
    bvoc_eq_ppm,
    (unsigned)iaq_accuracy,
    (unsigned)stab_status,
    (unsigned)run_in_status,
    gas_pct,
    raw_temperature_c,
    raw_humidity_pct,
    (unsigned)raw_gas_ohms,
    temperature_c,
    humidity_pct,
    gas_ohms,
    (unsigned)lux1_raw,
    (unsigned)lux2_raw,
    (unsigned)mic1_pp,
    (unsigned)mic2_pp,
    presence ? "true" : "false",
    (unsigned)people_count,
    (unsigned)target_distance_cm,
    rssi_dbm,
    uptime_s,
    timestamp_ms
  );

  if (n < 0 || n >= (int)sizeof(payload)) {
    Serial.println("[PUB] Erreur format JSON (buffer trop petit)");
    return;
  }

  Serial.printf("[PUB] Topic : %s\n", g_mqttTopic);
  Serial.printf("[PUB] JSON  : %s\n", payload);

  bool ok = mqtt.publish(g_mqttTopic, payload);
  Serial.printf("[PUB] Publish OK ? %s\n", ok ? "YES" : "NO");
}
/** @} */


/** @name Fonctions OTA
 *  Vérification et mise à jour du firmware à distance */
/** @{ */

/**
 * @brief Extrait une valeur String d'un JSON simple (sans bibliothèque)
 *
 * Recherche une clé au format "key":"value" dans une chaîne JSON
 * et retourne la valeur associée. Fonctionne sans dépendance à
 * ArduinoJson, adaptée aux manifestes OTA simples.
 *
 * @param json Chaîne JSON complète
 * @param key Nom de la clé à extraire (sans guillemets)
 * @param[out] out Référence String recevant la valeur
 * @return true si la clé a été trouvée et extraite, false sinon
 */
static bool jsonExtractString(const String& json, const char* key, String& out) {
  String pat = String("\"") + key + "\":";
  int i = json.indexOf(pat);
  if (i < 0) return false;
  i += pat.length();
  // sauter espaces
  while (i < (int)json.length() && (json[i] == ' ' || json[i] == '\t')) i++;
  if (i >= (int)json.length() || json[i] != '\"') return false;
  i++;
  int j = json.indexOf('\"', i);
  if (j < 0) return false;
  out = json.substring(i, j);
  return true;
}

/**
 * @brief Parse une chaîne de version SemVer (major.minor.patch)
 *
 * Extrait les trois composantes numériques d'une chaîne de version
 * au format SemVer. Gère les suffixes (beta, dev) en les ignorant.
 * Exemple : "1.2.3-beta" → major=1, minor=2, patch=3.
 *
 * @param v Chaîne de version à parser
 * @param[out] major Composante majeure
 * @param[out] minor Composante mineure
 * @param[out] patch Composante de correctif
 */
static void parseSemVer(const String& v, int& major, int& minor, int& patch) {
  major = minor = patch = 0;
  int p1 = v.indexOf('.');
  int p2 = v.indexOf('.', p1 + 1);
  String sMaj = (p1 > 0) ? v.substring(0, p1) : v;
  String sMin = (p1 > 0 && p2 > p1) ? v.substring(p1 + 1, p2) : "0";
  String sPat = (p2 > 0) ? v.substring(p2 + 1) : "0";
  // nettoyer suffixes (beta/dev)
  for (int k = 0; k < (int)sPat.length(); ++k) {
    if (!isDigit(sPat[k])) { sPat = sPat.substring(0, k); break; }
  }
  major = sMaj.toInt();
  minor = sMin.toInt();
  patch = sPat.toInt();
}

/**
 * @brief Compare deux chaînes de version SemVer
 *
 * Compare a et b composante par composante (major, minor, patch).
 * Utilisé pour déterminer si le firmware local est à jour ou
 * nécessite une mise à jour OTA.
 *
 * @param a Première version
 * @param b Seconde version
 * @return -1 si a < b, 0 si a == b, 1 si a > b
 */
static int compareSemVer(const String& a, const String& b) {
  int av1, av2, av3, bv1, bv2, bv3;
  parseSemVer(a, av1, av2, av3);
  parseSemVer(b, bv1, bv2, bv3);
  if (av1 != bv1) return (av1 < bv1) ? -1 : 1;
  if (av2 != bv2) return (av2 < bv2) ? -1 : 1;
  if (av3 != bv3) return (av3 < bv3) ? -1 : 1;
  return 0;
}

/**
 * @brief Convertit un tableau d'octets en chaîne hexadécimale
 *
 * Utilisé pour afficher l'empreinte SHA-256 du firmware téléchargé
 * et la comparer à celle attendue dans le manifeste OTA.
 *
 * @param buf Tableau d'octets
 * @param len Nombre d'octets
 * @return String Chaîne hexadécimale (minuscules)
 */
static String toHex(const uint8_t* buf, size_t len) {
  static const char* HEXCHARS = "0123456789abcdef";
  String s; s.reserve(len * 2);
  for (size_t i = 0; i < len; ++i) {
    s += HEXCHARS[(buf[i] >> 4) & 0xF];
    s += HEXCHARS[buf[i] & 0xF];
  }
  return s;
}

/**
 * @brief Vérifie le manifeste OTA et déclenche la mise à jour si nécessaire
 *
 * Télécharge le manifeste JSON depuis le serveur Noisyless, compare
 * les versions, et si une mise à jour est disponible, télécharge le
 * binaire, vérifie son empreinte SHA-256 et applique la mise à jour
 * via la bibliothèque Update d'ESP32. Protégé par mutex pour éviter
 * les appels concurrents.
 */
void checkOtaManifestAndUpdate() {
  // Mutex pour éviter deux OTA simultanées (WiFiClientSecure n'est pas thread-safe)
  if (g_otaMutex == nullptr) return;
  if (xSemaphoreTake(g_otaMutex, pdMS_TO_TICKS(10)) != pdTRUE) {
    Serial.println("[OTA] Bloqué par une autre instance");
    return;
  }
  if (g_otaRunning) {
    Serial.println("[OTA] Déjà en cours");
    xSemaphoreGive(g_otaMutex);
    return;
  }
  g_otaRunning = true;

  if (WiFi.status() != WL_CONNECTED) {
    g_otaRunning = false;
    xSemaphoreGive(g_otaMutex);
    return;
  }

  // Variables déclarées ici pour que goto ota_cleanup soit valide en C++
  WiFiClientSecure* client = nullptr;
  HTTPClient* https = nullptr;
  int code = 0;
  String body;
  String latest, minVersion, binUrl, sha256hex;
  mbedtls_sha256_context ctx;
  int len = 0;
  uint8_t buf[2048];
  size_t total = 0;
  bool shaStarted = false;
  WiFiClient* stream = nullptr;
  uint8_t digest[32];
  String digestHex;

  unsigned long ota_start_ms = millis();
  const unsigned long OTA_TIMEOUT_MS = 90000; // Timeout global de 90s pour OTA
  
  Serial.println("[OTA] Vérification manifest (stable)...");

  // Création sur le heap (évite stack corrompu par exception)
  client = new WiFiClientSecure();
  if (!client) {
    Serial.println("[OTA] new WiFiClientSecure failed");
    goto ota_cleanup;
  }
  client->setInsecure();

  https = new HTTPClient();
  if (!https) {
    Serial.println("[OTA] new HTTPClient failed");
    goto ota_cleanup;
  }
  https->setUserAgent(OTA_USER_AGENT);

  if (!https->begin(*client, OTA_MANIFEST_URL)) {
    Serial.println("[OTA] begin(manifest) échoué");
    goto ota_cleanup;
  }
  code = https->GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[OTA] Manifest HTTP %d\n", code);
    goto ota_cleanup;
  }
  body = https->getString();
  https->end();
  delete https; https = nullptr;
  delete client; client = nullptr;

  if (!jsonExtractString(body, "latest", latest) ||
      !jsonExtractString(body, "min", minVersion) ||
      !jsonExtractString(body, "bin", binUrl) ||
      !jsonExtractString(body, "sha256", sha256hex)) {
    Serial.println("[OTA] Manifest incomplet");
    goto ota_cleanup;
  }
  Serial.printf("[OTA] latest=%s min=%s\n", latest.c_str(), minVersion.c_str());

  if (compareSemVer(String(FW_VERSION), minVersion) < 0) {
    Serial.println("[OTA] Version locale trop ancienne pour min, ignorer.");
    goto ota_cleanup;
  }
  if (compareSemVer(String(FW_VERSION), latest) >= 0) {
    Serial.println("[OTA] Déjà à jour.");
    goto ota_cleanup;
  }

  // LED violette pendant OTA
  digitalWrite(PIN_LED_R, LOW);
  digitalWrite(PIN_LED_B, LOW);
  digitalWrite(PIN_LED_G, HIGH);

   // Téléchargement binaire et vérification SHA-256
   if (millis() - ota_start_ms > OTA_TIMEOUT_MS) {
     Serial.println("[OTA] Timeout atteint avant téléchargement");
     goto ota_cleanup;
   }
   client = new WiFiClientSecure();
   if (!client) goto ota_cleanup;
   client->setInsecure();

  https = new HTTPClient();
  if (!https) goto ota_cleanup;
  https->setUserAgent(OTA_USER_AGENT);

  if (!https->begin(*client, binUrl)) {
    Serial.println("[OTA] begin(bin) échoué");
    goto ota_cleanup;
  }
  code = https->GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[OTA] Bin HTTP %d\n", code);
    goto ota_cleanup;
  }
  len = https->getSize();
  stream = https->getStreamPtr();

  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0); // 0 = SHA-256
  shaStarted = true;

  if (!Update.begin(len > 0 ? len : UPDATE_SIZE_UNKNOWN)) {
    Serial.printf("[OTA] Update.begin err=%u\n", Update.getError());
    goto ota_cleanup;
  }
  total = 0;
  while (https->connected() && (len > 0 || len == -1)) {
    size_t avail = stream->available();
    if (!avail) { delay(1); continue; }
    size_t rd = stream->readBytes(buf, avail > sizeof(buf) ? sizeof(buf) : avail);
    if (rd == 0) break;
    total += rd;
    mbedtls_sha256_update_ret(&ctx, buf, rd);
    if (Update.write(buf, rd) != rd) {
      Serial.printf("[OTA] Ecriture échouée err=%u\n", Update.getError());
      Update.abort();
      goto ota_cleanup;
    }
    if (len > 0) len -= rd;
    if (millis() - ota_start_ms > OTA_TIMEOUT_MS) {
      Serial.println("[OTA] Timeout pendant le téléchargement");
      Update.abort();
      goto ota_cleanup;
    }
    yield(); // Nourrir le watchdog
  }
  https->end();
  delete https; https = nullptr;
  delete client; client = nullptr;

  mbedtls_sha256_finish_ret(&ctx, digest);
  mbedtls_sha256_free(&ctx);
  shaStarted = false;
  digestHex = toHex(digest, sizeof(digest));

  sha256hex.toLowerCase();
  digestHex.toLowerCase();
  if (digestHex != sha256hex) {
    Serial.printf("[OTA] SHA256 mismatch: got=%s expected=%s\n", digestHex.c_str(), sha256hex.c_str());
    Update.abort();
    goto ota_cleanup;
  }

  if (millis() - ota_start_ms > OTA_TIMEOUT_MS) {
    Serial.println("[OTA] Timeout après téléchargement avant finalisation");
    goto ota_cleanup;
  }
  if (!Update.end()) {
    Serial.printf("[OTA] Update.end err=%u\n", Update.getError());
    goto ota_cleanup;
  }
  if (Update.isFinished()) {
    Serial.printf("[OTA] MAJ vers %s OK, reboot...\n", latest.c_str());
    g_otaRunning = false;
    xSemaphoreGive(g_otaMutex);
    delay(200);
    ESP.restart();
  } else {
    Serial.println("[OTA] Update non finalisée");
  }

ota_cleanup:
  if (shaStarted) mbedtls_sha256_free(&ctx);
  if (https) { https->end(); delete https; }
  if (client) { delete client; }
  Serial.printf("[OTA] Terminé en %lu ms\n", millis() - ota_start_ms);
  g_otaRunning = false;
  xSemaphoreGive(g_otaMutex);
}
/** @} */


/** @name Fonctions Réseau
 *  Connexion WiFi (portail captif) et MQTT */
/** @{ */

/**
  * @brief Connexion au WiFi avec WiFiManager + portail captif
  *
  * Séquence de connexion en boucle jusqu'à succès :
  * 1. Si credentials NVS → auto-connect (15s)
  * 2. Sinon → captive portal "NOISYLESS-Setup" (60s)
  * 3. Timeout portal → fallback TP-Link_B150 hardcodé (20s)
  * 4. Fallback connecté → reboot (pour repartir proprement)
  * 5. Fallback échoué → retour au captive portal (boucle)
  *
  * Le module ne démarre pas sans WiFi.
  */
 void connectWiFi() {
   while (WiFi.status() != WL_CONNECTED) {
     digitalWrite(PIN_LED_R, LOW);
     digitalWrite(PIN_LED_G, HIGH);
     digitalWrite(PIN_LED_B, HIGH);

     prefs.begin(PREFS_NS, false); // read-only pour lire
     String savedSSID = prefs.getString("ssid", "");
     String savedPass = prefs.getString("pass", "");
     prefs.end();

     bool connected = false;

     // ---- Étape 1 : auto-connect NVS ----
     if (savedSSID.length() > 0) {
       Serial.printf("[WiFi] Auto-connect to saved AP \"%s\"\n", savedSSID.c_str());
       WiFi.mode(WIFI_STA);
       WiFi.begin(savedSSID.c_str(), savedPass.c_str());
       int dots = 0;
       while (WiFi.status() != WL_CONNECTED && dots < 30) { 
         delay(500); 
         Serial.print("."); 
         dots++;
         yield(); // WDT
       }
       if (WiFi.status() == WL_CONNECTED) {
         connected = true;
       } else {
         Serial.printf("\n[WiFi] ✗ Saved AP \"%s\" unavailable\n", savedSSID.c_str());
       }
     }

     // ---- Étape 2 : captive portal 60s ----
     if (!connected) {
       Serial.println("[WiFi] Starting captive portal 'NOISYLESS-Setup' (60s)...");
       wifiManager.setTitle("NOISYLESS");
       wifiManager.setConfigPortalTimeout(60);
       wifiManager.setCustomHeadElement(PROGMEM R"rawliteral(
 <style>
   *, *::before, *::after { box-sizing: border-box; }
   body { font-family:'IBM Plex Sans',sans-serif !important; background:#fff !important; color:#1a1a1a !important; margin:0; padding:24px; display:flex; flex-direction:column; align-items:center; justify-content:center; min-height:100vh; }
   .card { max-width:420px; width:100%; background:#fff; border:1px solid #e0e0e0; border-radius:12px; padding:32px 24px; }
   h2,h1 { font-family:'Space Mono',monospace !important; font-size:16px !important; font-weight:700 !important; text-align:center; text-transform:uppercase; letter-spacing:1px; }
   h1 { font-size:22px !important; letter-spacing:2px; margin-bottom:8px; }
   p { font-size:14px; color:#666; text-align:center; margin-bottom:24px; }
   label { display:block; font-size:12px; font-weight:600; text-transform:uppercase; letter-spacing:1px; color:#666; margin-bottom:6px; }
   input[type="text"],input[type="password"] { width:100%; padding:14px 16px; border:1px solid #d0d0d0; border-radius:8px; font-size:15px; color:#1a1a1a; background:#fafafa; }
   input:focus { outline:none; border-color:#1a1a1a; background:#fff; }
   button,input[type="submit"] { width:100%; padding:14px; background:#1a1a1a; color:#fff; border:none; border-radius:8px; font-family:'Space Mono',monospace; font-size:14px; font-weight:700; text-transform:uppercase; letter-spacing:1px; cursor:pointer; margin-top:12px; }
   button:hover,input[type="submit"]:hover { background:#333; }
   .msg { margin-top:16px; padding:12px; border-radius:8px; font-size:13px; text-align:center; }
   .msg.ok { background:#e8f5e9; color:#2e7d32; border:1px solid #c8e6c9; }
   .msg.err { background:#fbe9e7; color:#c62828; border:1px solid #ffccbc; }
   .logo { font-family:'Space Mono',monospace; font-size:22px; font-weight:700; letter-spacing:2px; text-transform:uppercase; text-align:center; margin-bottom:8px; color:#1a1a1a; }
 </style>
 <div class="logo">NOISYLESS</div>
 <p>Connectez votre capteur au WiFi</p>
 <div class="card">
 )rawliteral");
       wifiManager.setAPCallback([](WiFiManager* mgr) {
         Serial.println("[WiFiManager] Captive portal ACTIVE — connect to 'NOISYLESS-Setup'");
         digitalWrite(PIN_LED_R, HIGH);
         digitalWrite(PIN_LED_G, HIGH);
         digitalWrite(PIN_LED_B, LOW);
       });

        if (wifiManager.autoConnect("NOISYLESS-Setup")) {
          String newSSID = wifiManager.getWiFiSSID();
          String newPass = wifiManager.getWiFiPass();
          if (newSSID.length() > 0) {
            prefs.begin(PREFS_NS, false); // write mode
            prefs.putString("ssid", newSSID);
            prefs.putString("pass", newPass);
            prefs.end();
            Serial.printf("[WiFi] Saved credentials for \"%s\" to NVS\n", newSSID.c_str());
          }
          connected = true;
          Serial.println("\n[WiFi] ✓ Connected via captive portal");
        } else {
         // ---- Étape 3 : fallback TP-Link_B150 ----
         Serial.println("[WiFi] Captive portal timeout, fallback TP-Link_B150 (20s)...");
         WiFi.mode(WIFI_STA);
         WiFi.begin(WIFI_SSID, WIFI_PASS);
          int dots = 0;
          while (WiFi.status() != WL_CONNECTED && dots < 40) {
            delay(500);
            Serial.print(".");
            dots++;
            yield(); // WDT
          }
         if (WiFi.status() == WL_CONNECTED) {
           Serial.println("\n[WiFi] ✓ Fallback TP-Link_B150 connected — rebooting...");
           connected = true;
           prefs.end();
           delay(500);
           ESP.restart();  // reboot pour démarrer proprement avec ce WiFi
         } else {
           Serial.println("\n[WiFi] ✗ Fallback failed — restarting captive portal");
         }
       }
     }

      if (connected) {
        prefs.end();
        digitalWrite(PIN_LED_R, HIGH);
        digitalWrite(PIN_LED_B, HIGH);
        Serial.printf("[WiFi] IP  : %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
        
        // Séquence LED de version après WiFi connecté
        led_boot_sequence(FW_VERSION);
        
        return;
      }

     prefs.end();
     delay(1000);  // pause avant réessai
   }
 }

/**
 * @brief Connexion au broker MQTT
 *
 * Configure le serveur et les credentials MQTT (définis dans
 * credentials.h), puis tente la connexion. Les erreurs sont loguées
 * avec le code retour traduit par mqttError().
 */
void connectMQTT() {
  if (mqtt.connected()) return;

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  Serial.printf("\n[MQTT] Connexion à %s:%u ...\n", MQTT_HOST, MQTT_PORT);
  Serial.printf("[MQTT] ClientID : %s\n", CLIENT_ID);
  Serial.printf("[MQTT] User     : %s\n", MQTT_USER);

  if (mqtt.connect(CLIENT_ID, MQTT_USER, MQTT_PASS)) {
    Serial.println("[MQTT] Connecté");
  } else {
    Serial.printf("[MQTT] Échec rc=%d (%s)\n", mqtt.state(), mqttError(mqtt.state()));
  }
}
/** @} */


/** @name Fonctions Setup et Loop
 *  Point d'entrée du firmware et boucle principale */
/** @{ */

/**
 * @brief Setup principal du firmware Noisyless
 *
 * Initialise dans l'ordre :
 * 1. Console série et LEDs de statut
 * 2. Identifiant unique DEVICE_ID depuis l'adresse MAC
 * 3. Connexion WiFi (portail captif ou auto-connect)
 * 4. Configuration MQTT et topics
 * 5. Capteurs BSEC2 et LD2410
 * 6. Première connexion MQTT et publication immédiate
 * 7. Mutex partagé et vérification OTA au démarrage
 * 8. Création des tâches FreeRTOS :
 *    - task_mqtt : boucle réseau + publications périodiques + OTA
 *    - task_adc  : échantillonnage lumière et son
 *    - task_ld2410 : lecture radar avec tentative de ré-init
 *    - task_bsec : boucle BSEC2 run()
 */
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println();
  Serial.println("===== Noisyless Environmental Sensor =====");

  // LEDs éteintes (actif bas)
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  digitalWrite(PIN_LED_R, LOW);   // Rouge = boot
  digitalWrite(PIN_LED_G, HIGH);
  digitalWrite(PIN_LED_B, HIGH);

  // Générer DEVICE_ID unique depuis MAC
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  mac.toUpperCase();
  snprintf(DEVICE_ID, sizeof(DEVICE_ID), "NL-%s", mac.c_str());
  snprintf(CLIENT_ID, sizeof(CLIENT_ID), "noisyless_%s", DEVICE_ID);
  Serial.printf("[ID] DEVICE_ID=%s CLIENT_ID=%s\n", DEVICE_ID, CLIENT_ID);



  // Connexions
  Serial.println("connecting to WiFi...");
  connectWiFi();

  Serial.println("connecting to MQTT...");
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(1024);

  // Topic MQTT: utilise DEVICE_ID déjà généré depuis MAC
  // DEVICE_ID = "NL-XXXXXXXXXXXX" → MAC = les 12 derniers chars
  const char* macSuffix = DEVICE_ID + 3; // skip "NL-"
  strlcpy(g_macStr, macSuffix, sizeof(g_macStr));
  snprintf(g_mqttTopic, sizeof(g_mqttTopic), "esp32/noisyless/%s/datas", macSuffix);
  snprintf(g_mqttTopicLine, sizeof(g_mqttTopicLine), "esp32/noisyless/%s/line", macSuffix);

  Serial.println("initializing sensors...");
  // Capteurs
  
  g_bsec_ok = initBSEC();
  if (!g_bsec_ok) {
    Serial.println("[BSEC2] Attention: initialisation BSEC2 échouée.");
  }
  Serial.println("BSEC2 initialized");
  g_lastBsecInitAttemptMs = millis();

  g_ld_ok = initLD2410();

  // Première connexion MQTT
    connectMQTT();
  Serial.println("MQTT connected");
  
  // ====== PUBLICATION IMMÉDIATE ======
  // Publish tout de suite après MQTT pour que le broker nous voie
  // avant l'OTA check qui peut prendre 5-30s et faire timeout MQTT
  Serial.println("[BOOT] Publication immédiate après MQTT...");
  publishMeasurements();
  // ===================================
  
  digitalWrite(PIN_LED_R, HIGH);
  digitalWrite(PIN_LED_G, HIGH);
  digitalWrite(PIN_LED_B, HIGH);

  // Mutex partagé et OTA (AVANT premier OTA check)
  g_sharedMutex = xSemaphoreCreateMutex();
  g_otaMutex = xSemaphoreCreateMutex();

  // ====== FORCE OTA CHECK AT BOOT ======
  Serial.println("[OTA] Forcing OTA check at boot...");
  g_otaRunning = true; // Blockera le premier check concurrent
  checkOtaManifestAndUpdate();
  // =====================================

  // ====== FreeRTOS Tasks ======
  // Tâche MQTT (loop + publication périodique + OTA périodique)
  xTaskCreatePinnedToCore(
    [](void*) {
      unsigned long lastPub = 0;
      unsigned long lastOta = 0;
      for (;;) {
        if (WiFi.status() != WL_CONNECTED) connectWiFi();
        if (!mqtt.connected()) connectMQTT();
        mqtt.loop();
        unsigned long now = millis();
        if (now - lastPub >= PUBLISH_INTERVAL_MS) {
          lastPub = now;
          publishMeasurements();
        }
        if (now - lastOta >= OTA_CHECK_INTERVAL_MS && !g_otaRunning) {
          lastOta = now;
          checkOtaManifestAndUpdate();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    },
    "task_mqtt",
    8192, nullptr, 1, nullptr, 1);

  // Tâche ADC (lumière + son)
  xTaskCreatePinnedToCore(
    [](void*) {
      for (;;) {
        uint16_t l1, l2, m1, m2;
        readLightSensors(l1, l2);
        readMicrophones(m1, m2);
        if (g_sharedMutex && xSemaphoreTake(g_sharedMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
          g_shared.lux1_raw = l1;
          g_shared.lux2_raw = l2;
          g_shared.mic1_pp = m1;
          g_shared.mic2_pp = m2;
          xSemaphoreGive(g_sharedMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    },
    "task_adc",
    4096, nullptr, 1, nullptr, 1);

  // Tache RADAR LD2410 (avec re-essai d'init si echec au boot)
  xTaskCreatePinnedToCore(
    [](void*) {
      unsigned long lastInitTry = 0;
      for (;;) {
        if (!g_ld_ok) {
          unsigned long now = millis();
          if (now - lastInitTry >= 5000UL) {
            lastInitTry = now;
            g_ld_ok = initLD2410();
          }
        } else {
          bool pres; uint16_t dist;
          readLD2410(pres, dist);
          if (g_sharedMutex && xSemaphoreTake(g_sharedMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            g_shared.presence = pres;
            g_shared.distance_cm = dist;
            xSemaphoreGive(g_sharedMutex);
          }
        }
        vTaskDelay(pdMS_TO_TICKS(RADAR_READ_PERIOD_MS));
      }
    },
    "task_ld2410",
    4096, nullptr, 1, nullptr, 0);

  // Tâche BSEC2 (run) — uniquement si l'init a réussi
  xTaskCreatePinnedToCore(
    [](void*) {
      for (;;) {
        if (g_bsec_ok) {
          if (!envSensor.run()) {
            checkBsecStatus(envSensor);
          } else {
            if (g_sharedMutex && xSemaphoreTake(g_sharedMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
              g_shared.bsec.valid          = g_bsec_last.valid;
              g_shared.bsec.temp_comp_c    = g_bsec_last.temp_comp_c;
              g_shared.bsec.hum_comp_pct   = g_bsec_last.hum_comp_pct;
              g_shared.bsec.pressure_pa    = g_bsec_last.pressure_pa;
              g_shared.bsec.gas_ohms       = g_bsec_last.gas_ohms;
              g_shared.bsec.raw_temp_c     = g_bsec_last.raw_temp_c;
              g_shared.bsec.raw_hum_pct    = g_bsec_last.raw_hum_pct;
              g_shared.bsec.raw_gas_ohms   = g_bsec_last.raw_gas_ohms;
              g_shared.bsec.iaq            = g_bsec_last.iaq;
              g_shared.bsec.s_iaq          = g_bsec_last.s_iaq;
              g_shared.bsec.co2eq_ppm      = g_bsec_last.co2eq_ppm;
              g_shared.bsec.bvoc_eq_ppm    = g_bsec_last.bvoc_eq_ppm;
              g_shared.bsec.gas_pct        = g_bsec_last.gas_pct;
              g_shared.bsec.iaq_accuracy   = g_bsec_last.iaq_accuracy;
              g_shared.bsec.stab_status    = g_bsec_last.stab_status;
              g_shared.bsec.run_in_status  = g_bsec_last.run_in_status;
              xSemaphoreGive(g_sharedMutex);
            }
          }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
      }
    },
    "task_bsec",
    6144, nullptr, 1, nullptr, 0);
}

/**
 * @brief Boucle principale (idle)
 *
 * La boucle principale ne fait rien : toutes les opérations sont
 * déléguées aux tâches FreeRTOS créées dans setup().
 * Cette fonction reste présente pour satisfaire le framework Arduino.
 */
void loop() {
  vTaskDelay(pdMS_TO_TICKS(100)); // boucle idle, les tâches gèrent tout
}
/** @} */
