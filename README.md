# NOISYLESS — Capteur Environnemental IoT

Firmware ESP32-S3 pour le capteur environnemental NOISYLESS. Monitoring en temps réel du bruit, de la luminosité, et de la qualité d'air — transmission MQTT vers le dashboard, OTA over-the-air, LED de statut.

## Les 3 types de capteurs

| Capteur | Pins | Mesure | Méthode |
|---------|------|--------|---------|
| 🔆 **Luminosité 1** | GPIO1 (ADC) | Lux ambiant direction 1 | `analogRead()` 12-bit |
| 🔆 **Luminosité 2** | GPIO2 (ADC) | Lux ambiant direction 2 | `analogRead()` 12-bit |
| 🎤 **Bruit (micro × 2)** | GPIO3 + GPIO4 (ADC) | Niveau sonore crête-à-crête | Échantillonnage 50ms, peak-to-peak |

**Principe** : le capteur NOISYLESS mesure simultanément le bruit ambiant (deux micros électret analogiques) et la luminosité (deux photorésistances). Les valeurs brutes sont publiées toutes les 10 secondes via MQTT sur le serveur. Le dashboard agrège et affiche les données en temps réel. Le 3ᵉ axe — **qualité d'air (IAQ/température/humidité/pression)** via BME688 — est réservé pour une version ultérieure.

## Séquence LED

| État | LED | Durée |
|------|-----|-------|
| Boot | 🔴 Rouge fixe | ~1s |
| WiFi en cours | 🔵 Bleu fixe | Jusqu'à connexion |
| Version | 🔵×N clignotements | N = 3ᵉ chiffre version (ex: v1.0.**8** → 8 flashs) |
| Connecté | 🟢 Vert fixe | 1s |
| Publication MQTT OK | 🟢 Flash vert | 200ms |
| Idle | ⚫ Éteinte | — |
| WiFi perdu | 🔴 Rouge fixe | Jusqu'à reconnexion |

**Mapping GPIO** : LED_R=GPIO12, LED_G=GPIO13, LED_B=GPIO14. Actif bas : `LOW = allumé`.

## Déploiement OTA — Workflow complet

### Prérequis

- PlatformIO installé (`pio --version`)
- Accès SSH au serveur Hetzner : `noisyless@91.99.26.43`
- Firmware servi par nginx depuis `/opt/noisyless/static/firmware/` sur le port 80
- Les modules ESP32 doivent avoir le WiFi hardcodé dans `include/wifi_config.h`

### 1. Modifier le firmware

```bash
cd 05_FIRMWARE
```

Sources à modifier :
- `src/main.cpp` — logique métier, OTA, LED, capteurs
- `include/wifi_config.h` — SSID + mot de passe WiFi
- `include/credentials.h` — host/user/pass MQTT

**Incrémenter `FW_VERSION`** dans `main.cpp` à chaque déploiement :
```cpp
const char* FW_VERSION = "1.0.9";  // ← changer ici
```

### 2. Builder

```bash
pio run -e esp32dev
```

Sortie : `.pio/build/esp32dev/firmware.bin` (~879 Ko pour S3 sans PSRAM)

**Vérifier la config PlatformIO** (`platformio.ini`) :
- `board = esp32-s3-devkitc-1`
- `board_build.flash_size = 8MB`
- `board_build.psram_type = disabled`
- `board_build.flash_mode = dio`
- `board_build.flash_freq = 40m`
- `build_flags = -D ARDUINO_USB_CDC_ON_BOOT=1`

### 3. Calculer le SHA256

```bash
sha256sum .pio/build/esp32dev/firmware.bin
# Exemple : 07a0d65f234ddf75f67a57086c47439a90afa1520df972ebf58945e3caed91b1
```

### 4. Déployer sur le serveur

```bash
# Copier le binaire
scp .pio/build/esp32dev/firmware.bin noisyless@91.99.26.43:/opt/noisyless/static/firmware/

# Mettre à jour le manifest OTA (sur le serveur)
ssh noisyless@91.99.26.43 'cat > /opt/noisyless/static/firmware/latest.json << EOF
{"version":"1.0.9","url":"http://91.99.26.43/firmware/firmware.bin","sha256":"LE_HASH_ICI"}
EOF'
```

### 5. Vérifier

```bash
# Manifest accessible
curl -s http://91.99.26.43/firmware/latest.json
# → {"version":"1.0.9","url":"http://91.99.26.43/firmware/firmware.bin","sha256":"..."}

# Binaire accessible (doit retourner 200)
curl -sI http://91.99.26.43/firmware/firmware.bin | head -3
# → HTTP/1.1 200 OK, Content-Length: 879520

# Vérifier le SHA256 côté serveur
ssh noisyless@91.99.26.43 'sha256sum /opt/noisyless/static/firmware/firmware.bin'
# → Doit correspondre au hash local
```

### 6. Propagation OTA automatique

Chaque module ESP32 checke `http://91.99.26.43/firmware/latest.json` **toutes les 60 secondes**. Si la version du manifest diffère de `FW_VERSION` compilée :

1. Téléchargement du `.bin` (30s timeout)
2. Vérification SHA256
3. `Update.begin()` → écriture flash
4. `ESP.restart()`

**Rollback automatique** : si le nouveau firmware ne boote pas, le bootloader ESP32-S3 restaure l'ancienne partition. Le module repasse silencieusement à la version précédente.

### 7. Surveillance

```bash
# Voir les devices et leur version firmware
ssh noisyless@91.99.26.43 "docker compose -f /opt/noisyless/docker-compose.yml exec -T timescaledb psql -U noisyless -d noisyless -c 'SELECT device_id, firmware_version, status, last_seen FROM noisyless.devices ORDER BY last_seen DESC;'"

# Voir les logs env-ingest (publishes MQTT)
ssh noisyless@91.99.26.43 "docker compose -f /opt/noisyless/docker-compose.yml logs --tail=30 env-ingest | grep -E 'fw_version|device_id'"
```

### Flash USB direct (1er module ou recovery)

```bash
# Brancher l'ESP32-S3 en USB, puis :
pio run -e esp32dev -t upload

# En cas de bootloop : full erase avant flash
pio run -e esp32dev -t erase && pio run -e esp32dev -t upload
```

## Payload MQTT

Topic : `esp32/noisyless/{MAC}/datas`

```json
{
  "product": "noisyless_env",
  "client_code": "client_demo",
  "device_id": "NL-A1B2C3D4E5F6",
  "fw_version": "1.0.8",
  "lux1_raw": 2048,
  "lux2_raw": 1890,
  "sound_mic1_pp": 295,
  "sound_mic2_pp": 266,
  "rssi_dbm": -58,
  "uptime_s": 3600
}
```

## Architecture

```
┌──────────────────────┐     MQTT      ┌─────────────────┐     SQL      ┌──────────────┐
│  ESP32-S3 (×N)       │──────────────▶│  Hetzner VPS     │─────────────▶│ TimescaleDB   │
│  WiFi + MQTT + OTA   │   port 1883   │  mosquitto       │             │               │
│  lux ×2 + mic ×2     │               │  env-ingest      │    HTTP     │ devices       │
│  LED RGB statut      │◀──────────────│  api             │◀────────────│ measurements │
└──────────────────────┘   OTA HTTP:80  │  nginx (static)  │             └──────────────┘
                                        └─────────────────┘
```

## Pièges connus

- **ESP32-S3 flash mode** : le S3-MINI-1 est QIO natif mais GPIO12/14 sont sur le bus SPI → utiliser DIO. Sans ça, les LEDs ne fonctionnent pas.
- **Partitions OTA** : `default.csv` suffit. `custom_partitions.csv` peut causer des bootloops si l'otadata est corrompue → full erase nécessaire.
- **WiFiManager vs WiFi dur** : le firmware standard utilise `WiFi.begin()` direct. Les credentials WiFiManager persistants en NVS survivent au reflash → `esptool.py erase_flash` si on change de WiFi.
- **LED jaune** : si on allume le vert sans éteindre le rouge d'abord → les deux canaux sont actifs simultanément → jaune. Toujours `ledOff()` avant de changer de couleur.
- **SHA256 mismatch** : si le hash dans le manifest ne correspond pas au binaire, l'OTA est silencieusement ignoré. Vérifier avec `curl -s http://91.99.26.43/firmware/firmware.bin | sha256sum`.
- **WiFi out of range** : si le module perd le WiFi, la LED passe au rouge et il tente une reconnexion à chaque itération de `loop()` (~10ms). L'OTA ne se déclenche que si WiFi connecté.
