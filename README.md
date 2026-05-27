# NOISYLESS — Capteur Environnemental IoT

Firmware ESP32-S3 modulaire pour le capteur environnemental NOISYLESS. Monitoring en temps réel du bruit, de la luminosité, de la qualité d'air (IAQ/CO₂/VOC) et de la présence humaine — transmission MQTT vers le dashboard, OTA over-the-air, LED de statut RGB.

## Capteurs (5 axes)

| Capteur | Pins | Mesure | Module |
|---------|------|--------|--------|
| 🔆 **Luminosité ×2** | GPIO1, GPIO2 (ADC 12-bit) | Lux ambiant, deux directions | `analog_sensors.h` |
| 🎤 **Bruit (micro ×2)** | GPIO3, GPIO4 (ADC 12-bit) | Niveau sonore crête-à-crête (peak-to-peak 50ms) | `analog_sensors.h` |
| 🌡️ **Qualité d'air** | I²C SDA=GPIO35, SCL=GPIO36 | IAQ, CO₂eq, bVOCeq, T°/Hum/Pression compensées, Gas | `bme680_sensor.h` |
| 📡 **Présence radar** | UART Serial1 RX=GPIO18, TX=GPIO17 (256000 bauds) | Cible mobile, cible stationnaire, distance cm | `ld2410_sensor.h` |

### Principe de mesure

- **Lux ×2** : lecture directe `analogRead()` 12-bit, deux photorésistances orientées
- **Micro ×2** : échantillonnage 50ms, calcul de l'amplitude crête-à-crête (`max - min`) via deux électrets analogiques
- **BME680 + BSEC2** : capteur I²C 0x76, mode Ultra Low Power (ULP) — mesure toutes les 3s, calibration IAQ continue via algorithme Bosch BSEC2 (IAQ 0-500, CO₂eq, bVOCeq, T°/Hum/Pression compensées)
- **LD2410** : radar 24 GHz sur UART 256000 bauds, parse les trames binary → détection mouvement + stationnaire + distance

## Architecture firmware (v2.0.0)

```
05_FIRMWARE/
├── src/
│   ├── main.cpp                     # setup(), tâches FreeRTOS
│   ├── sensors/
│   │   ├── bme680_sensor.h/.cpp     # BME680 + BSEC2 (IAQ, CO2, VOC, T/H/P)
│   │   ├── ld2410_sensor.h          # LD2410 radar (présence, distance)
│   │   └── analog_sensors.h         # Lux ×2 + micros ×2 (ADC peak-to-peak)
│   ├── comm/
│   │   └── wifi_mqtt.h              # WiFi (hardcodé) + MQTT (PubSubClient)
│   ├── ota/
│   │   └── ota_manager.h            # OTA HTTP check 60s + download + flash SHA256
│   └── ui/
│       └── led_status.h/.cpp        # LED RGB (actif bas), séquence boot, timings
├── include/
│   ├── wifi_config.h                # SSID + mot de passe WiFi
│   └── credentials.h                # MQTT host/user/pass
└── platformio.ini                   # ESP32-S3, DIO, BSEC2 + LD2410 + PubSubClient
```

### FreeRTOS — 3 tâches

| Tâche | Cœur | Priorité | Stack | Rôle |
|-------|------|----------|-------|------|
| `task_mqtt` | 0 | 1 | 4 Ko | WiFi/MQTT/OTA + publication 10s + LED rouge si WiFi perdu |
| `task_sensors` | 1 | 2 | 4 Ko | BME680 tick + LD2410 tick + Analog (50ms) |
| `task_led` | 0 | 0 | 2 Ko | Timings LED (vert boot 1s, flash publish 200ms) |

Les données sont partagées via `SharedData` protégé par `SemaphoreHandle_t` mutex.

### Séquence LED

| État | LED | Durée |
|------|-----|-------|
| Boot | 🔴 Rouge fixe | ~1s |
| WiFi en cours | 🔵 Bleu fixe | Jusqu'à connexion |
| Version | 🔵×N clignotements | N = 3ᵉ chiffre version (ex: v2.0.**0** → 10 flashs) |
| Connecté | 🟢 Vert fixe | 1s |
| Publication MQTT OK | 🟢 Flash vert | 200ms |
| Idle | ⚫ Éteinte | — |
| WiFi perdu | 🔴 Rouge fixe | Jusqu'à reconnexion |

**Mapping GPIO** : LED_R=GPIO12, LED_G=GPIO13, LED_B=GPIO14. Actif bas : `LOW = allumé`.

## Payload MQTT

Topic : `esp32/noisyless/{MAC}/datas`

```json
{
  "product": "noisyless_env",
  "client_code": "client_demo",
  "device_id": "NL-A1B2C3D4E5F6",
  "fw_version": "2.0.0",

  "lux1_raw": 2048,
  "lux2_raw": 1890,
  "sound_mic1_pp": 295,
  "sound_mic2_pp": 266,

  "presence": true,
  "distance_cm": 120,

  "temp_comp_c": 22.40,
  "hum_comp_pct": 48.30,
  "pressure_pa": 101325,
  "iaq": 25.0,
  "co2eq_ppm": 450.0,
  "bvoc_eq_ppm": 0.50,
  "gas_ohms": 50000,
  "iaq_accuracy": 3,
  "stab_status": 1,
  "run_in_status": 1,

  "rssi_dbm": -58,
  "uptime_s": 3600
}
```

**Champs optionnels** : si le BME680 n'est pas détecté, les champs IAQ/CO₂/VOC/T°/Hum/Pression sont omis. Si le LD2410 n'est pas détecté, `presence` et `distance_cm` restent à `false`/`0`.

## Déploiement OTA — Workflow complet

### Prérequis

- PlatformIO (`pio --version`)
- Accès SSH au serveur Hetzner : `noisyless@91.99.26.43`
- Firmware servi par nginx depuis `/opt/noisyless/static/firmware/` sur le port 80
- WiFi hardcodé dans `include/wifi_config.h`

### 1. Modifier le firmware

```bash
cd 05_FIRMWARE
```

Sources à modifier :
- `src/main.cpp` — `FW_VERSION`, logique métier
- `include/wifi_config.h` — SSID + mot de passe WiFi
- `include/credentials.h` — host/user/pass MQTT

**Incrémenter `FW_VERSION`** dans `main.cpp` :
```cpp
#define FW_VERSION "2.0.1"  // ← changer ici
```

### 2. Builder

```bash
pio run -e esp32dev
```

Sortie : `.pio/build/esp32dev/firmware.bin` (~972 Ko avec BSEC2+LD2410)

**Config PlatformIO** (`platformio.ini`) :
- `board = esp32-s3-devkitc-1`
- `board_build.flash_size = 8MB`
- `board_build.psram_type = disabled`
- `board_build.flash_mode = dio`
- `board_build.flash_freq = 40m`
- `build_flags = -D ARDUINO_USB_CDC_ON_BOOT=1`
- `lib_deps = PubSubClient, ld2410, bsec2, BME68x`

### 3. Calculer le SHA256

```bash
sha256sum .pio/build/esp32dev/firmware.bin
```

### 4. Déployer sur le serveur

```bash
# Copier le binaire
scp .pio/build/esp32dev/firmware.bin noisyless@91.99.26.43:/opt/noisyless/static/firmware/

# Mettre à jour le manifest OTA
ssh noisyless@91.99.26.43 'cat > /opt/noisyless/static/firmware/latest.json << EOF
{"version":"2.0.1","url":"http://91.99.26.43/firmware/firmware.bin","sha256":"LE_HASH_ICI"}
EOF'
```

### 5. Vérifier

```bash
# Manifest
curl -s http://91.99.26.43/firmware/latest.json

# Binaire (200 OK)
curl -sI http://91.99.26.43/firmware/firmware.bin | head -3

# SHA256 côté serveur
ssh noisyless@91.99.26.43 'sha256sum /opt/noisyless/static/firmware/firmware.bin'
```

### 6. Propagation OTA

Chaque module checke le manifest **toutes les 60s**. Si `version != FW_VERSION` :
1. Téléchargement `.bin` (30s timeout)
2. Vérification SHA256
3. `Update.begin()` → flash
4. `ESP.restart()`

**Rollback automatique** : si le firmware ne boote pas, le bootloader ESP32-S3 restaure la partition précédente.

### 7. Surveillance

```bash
# Devices et versions
ssh noisyless@91.99.26.43 "docker compose -f /opt/noisyless/docker-compose.yml exec -T timescaledb psql -U noisyless -d noisyless -c 'SELECT device_id, firmware_version, status, last_seen FROM noisyless.devices ORDER BY last_seen DESC;'"

# Logs env-ingest
ssh noisyless@91.99.26.43 "docker compose -f /opt/noisyless/docker-compose.yml logs --tail=30 env-ingest | grep -E 'fw_version|device_id'"
```

### Flash USB direct (recovery)

```bash
pio run -e esp32dev -t upload

# Bootloop → full erase
pio run -e esp32dev -t erase && pio run -e esp32dev -t upload
```

## Architecture système

```
┌──────────────────────────┐     MQTT      ┌─────────────────┐     SQL      ┌──────────────┐
│  ESP32-S3 (×N)           │──────────────▶│  Hetzner VPS     │─────────────▶│ TimescaleDB   │
│  WiFi + MQTT + OTA       │   port 1883   │  mosquitto       │             │               │
│  lux×2 + mic×2           │               │  env-ingest      │    HTTP     │ devices       │
│  BME680/BSEC2 IAQ        │               │  api             │◀────────────│ measurements │
│  LD2410 présence         │◀──────────────│  nginx (static)  │             └──────────────┘
│  LED RGB statut          │   OTA HTTP:80  │                  │
└──────────────────────────┘               └─────────────────┘
```

## Pièges connus

- **ESP32-S3 flash mode** : S3-MINI-1 est QIO natif mais GPIO12/14 sont sur le bus SPI → utiliser DIO 40 MHz. Sans ça, les LEDs ne fonctionnent pas.
- **Partitions OTA** : `default.csv` suffit. `custom_partitions.csv` peut causer des bootloops si l'otadata est corrompue.
- **WiFiManager NVS** : les credentials WiFiManager survivent au reflash. Utiliser `esptool.py erase_flash` si on change de WiFi.
- **LED jaune** : toujours `led_off()` avant de changer de couleur (actif bas, rouge+vert simultanés = jaune).
- **SHA256 mismatch** : OTA silencieusement ignoré si le hash manifest ≠ hash binaire. Vérifier avec `curl | sha256sum`.
- **BSEC2 calibration** : l'IAQ met ~5 min à se stabiliser au premier boot (`run_in_status` = 1). L'accuracy monte progressivement de 0 à 3.
- **LD2410 baud rate** : 256000 obligatoire. Le constructeur documente 115200 mais le firmware usine est à 256000.
- **BME680 I²C** : scanne 0x76 puis 0x77. Si le capteur est en mode sleep (broche MODE flottante), il ne répond pas. GPIO37 doit être LOW.
- **Taille flash** : BSEC2 + BME68x + LD2410 = ~972 Ko / 1310 Ko dispo (74%). Compiler sans PSRAM. Vérifier `board_build.psram_type = disabled`.
