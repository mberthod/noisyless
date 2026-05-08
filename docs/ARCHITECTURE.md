# NOISYLESS — Architecture & Pipeline de donnees

**Derniere mise a jour** : 2026-05-08
**Etat** : MVP fonctionnel end-to-end

---

## 1. Vue d'ensemble

```
+------------------+        WiFi          +-----------------+
| ESP32-S3 capteur | <------------------> | Reseau client    |
| BME680 + LD2410  |                      +-----------------+
| 2x mic + 2x lux  |                              |
+------------------+                              |
        |                                         v
        | MQTT plain (port 1883)         +-----------------+
        | Topic: esp32/noisyless/        | Internet        |
        |        <MAC>/datas             +-----------------+
        v                                         |
+------------------+                              v
| VPS Hetzner      |   <----  91.99.26.43  ------+
| (platform.       |
|  noisyless.com)  |
+------------------+
        |
        | Docker network "noisyless_default"
        |
        +---> Mosquitto (1883)  -+
        |                        |
        |                        v
        +---> mqtt_ingest.py ----+
        |        |
        |        v
        +---> TimescaleDB (5432, table measurements)
        |
        +---> FastAPI (interne 8000) <-- Nginx (443) <-- HTTPS clients
```

---

## 2. Pipeline de donnees

### 2.1 Firmware ESP32-S3

**Fichier principal** : `05_FIRMWARE/src/main.cpp`

**Capteurs lus** :
| Capteur | Bus | Pins | Donnees |
|---------|-----|------|---------|
| BME680 | I2C | SDA=35, SCL=36 | Temp, humidite, pression, gaz, IAQ, CO2eq, VOC (via BSEC2) |
| LD2410 | UART1 | RX=18, TX=17 @256000 baud | Presence, distance moving/stationary, energy |
| Mic 1 | ADC analog | GPIO 3 | Pic-a-pic 50ms |
| Mic 2 | ADC analog | GPIO 4 | Pic-a-pic 50ms |
| Lux 1 | ADC analog | GPIO 1 | Brut 0-4095 |
| Lux 2 | ADC analog | GPIO 2 | Brut 0-4095 |

**Tasks FreeRTOS** (dual-core ESP32-S3) :
- `task_mqtt` — connexion WiFi + MQTT, publish toutes les 10s, check OTA toutes les heures
- `task_adc` — lecture lux + mic toutes les 100ms
- `task_ld2410` — lecture frames radar toutes les 100ms (parser custom)
- `task_bsec` — execute BSEC2 toutes les 50ms

**Format JSON publie** (toutes les 10s) :
```json
{
  "product": "noisyless_env",
  "device_id": "NL-001",
  "fw_version": "1.0.0",
  "temperature_c": 27.47,
  "humidity_pct": 43.07,
  "pressure_pa": 95867,
  "gas_ohms": 59724,
  "iaq": 50.0,
  "co2eq_ppm": 500.0,
  "bvoc_eq_ppm": 0.5,
  "lux1_raw": 1014,
  "lux2_raw": 1158,
  "sound_mic1_pp": 251,
  "sound_mic2_pp": 0,
  "presence": true,
  "target_distance_cm": 30,
  "rssi_dbm": -47,
  "uptime_s": 3373
}
```

**Topic MQTT** : `esp32/noisyless/<MAC>/datas` (MAC sans deux-points, hex maj.)

### 2.2 Mosquitto (broker MQTT)

**Container** : `noisyless-mosquitto-1` sur VPS
**Config** : `/opt/noisyless/mosquitto/config/mosquitto.conf`

```
listener 1883
allow_anonymous false
password_file /mosquitto/config/passwd
persistence true
persistence_location /mosquitto/data/
```

**Auth** : user `esp32` / pass dans `/opt/noisyless/mosquitto/config/passwd`
**Port expose** : 1883 sur l'IP publique du VPS (Hetzner sans firewall par defaut)
**TLS 8883** : non actif au MVP, prevue pour Phase 2 (clients externes)

### 2.3 mqtt_ingest.py (pont MQTT -> DB)

**Container** : `noisyless-mqtt-ingest`
**Image** : buildee localement depuis `/opt/noisyless/mqtt-ingest/`

**Role** :
1. Subscribe au topic `esp32/noisyless/+/datas`
2. Parse le JSON
3. Convertit `sound_mic1_pp` -> dB SPL (formule log)
4. Convertit `lux1_raw` -> lux (lineaire)
5. INSERT dans `measurements` (TimescaleDB)
6. UPDATE `devices.last_seen` + `firmware_version`

**Conversions appliquees** :
- `db_avg = 30 + 20*log10(max(pp, 1))`
- `db_peak = 30 + 20*log10(max(max(mic1, mic2), 1))`
- `lux = raw_adc * 0.25`

**Code** : `/home/mathieu/noisyless/deploy/mqtt-ingest/mqtt_ingest.py`

### 2.4 TimescaleDB (table measurements)

**Container** : `noisyless-timescaledb-1`
**DB** : `noisyless`, user `noisyless`
**Schema** :
```sql
CREATE TABLE measurements (
    time TIMESTAMPTZ NOT NULL,
    device_id VARCHAR(20) REFERENCES devices(device_id),
    temp NUMERIC(5,2),
    humidity NUMERIC(5,2),
    gas_resistance NUMERIC(10,2),
    iaq INTEGER,
    presence BOOLEAN,
    distance_cm NUMERIC(5,2),
    db_avg NUMERIC(5,2),
    db_peak NUMERIC(5,2),
    lux1 NUMERIC(5,2),
    lux2 NUMERIC(5,2),
    PRIMARY KEY (time, device_id)
);
SELECT create_hypertable('measurements', 'time');
```

`devices.firmware_version` ajoute via `ALTER TABLE` le 2026-05-08.

### 2.5 API FastAPI

**Container** : `noisyless-api-1`
**Endpoints utilises par le dashboard** :
- `POST /api/auth/send-magic-link` — auth email-only
- `POST /api/auth/verify-magic-link` — retourne JWT
- `GET /api/devices` — liste capteurs lies au user (avec dernier etat)
- `GET /api/devices/{id}/live` — derniere mesure (format imbrique)
- `GET /api/devices/{id}/timeline` — 24h d'historique
- `POST /api/devices` — lier un nouveau capteur

**Format reponse `/live`** (corrige 2026-05-08) :
```json
{
  "device_id": "NL-001",
  "last_seen": "2026-05-08T13:16:03Z",
  "bme": { "temp": 27.47, "humidity": 43.07, "iaq": 50, "gas_resistance": 59724 },
  "ld2410": { "presence": true, "distance_cm": 30 },
  "mic": { "db_avg": 78.1, "db_peak": 78.1 },
  "lux1": 1.2,
  "lux2": 11.2
}
```

### 2.6 Dashboard frontend

**Fichiers** : `/opt/noisyless/static/{index,verify,dashboard}.html`
**Tech** : Vue 3 (CDN), CSS pur, pas de build step

**Bug corrige le 2026-05-08** : le dashboard lisait `d.live.temp` (plat) alors que l'API retourne `d.live.bme.temp` (imbrique). Toutes les references corrigees pour utiliser le format imbrique.

---

## 3. Modifications firmware (2026-05-08)

### 3.1 MQTT broker pointe vers VPS

**Avant** : `MQTT_HOST = "85.215.145.44"` (mauvais serveur)
**Apres** : `MQTT_HOST = "91.99.26.43"` (VPS NOISYLESS)

Fichier : `05_FIRMWARE/include/credentials.h`

### 3.2 Parser LD2410 custom (remplace lib buggee)

**Probleme** : `ncmreynolds/ld2410@^0.1.4` retournait `false` sur `radar.begin()` alors que le capteur envoyait bien des frames.

**Diagnostic** : lecture brute UART a montre des frames valides au format datasheet HLK-LD2410 :
```
F4 F3 F2 F1 [header] | LL LL [length] | 02 AA [type] | STATE | MD_L MD_H | ME | SD_L SD_H | SE | DD_L DD_H | 55 | CK | F8 F7 F6 F5 [footer]
```

**Solution** : parser custom (`parseLD2410Stream()`) ~50 lignes qui scanne les frames F4F3F2F1...F8F7F6F5. La lib est toujours dans `platformio.ini` mais `radar.read()` n'est plus appele.

**Champs lus** :
- `state` : 0=aucun, 1=mouvement, 2=stationnaire, 3=les deux
- `moving_dist_cm` + `moving_energy`
- `still_dist_cm` + `still_energy`
- `detection_dist_cm`

### 3.3 Re-essai d'init en boucle (5s)

L'init du LD2410 ne fait plus echec definitif. La task `task_ld2410` re-essaie toutes les 5s si l'init echoue (utile au boot si le capteur met du temps a demarrer).

---

## 4. Deploiement VPS — comment ca a ete fait

### 4.1 Service mqtt_ingest

```bash
# Local
cd /home/mathieu/noisyless/deploy
bash deploy-vps.sh
```

Le script :
1. Upload `mqtt-ingest/` sur `/opt/noisyless/mqtt-ingest/`
2. Upload `mosquitto/mosquitto.conf` sur le VPS
3. Cree le fichier passwd Mosquitto via container temp (avec bonnes permissions UID 1883)
4. Genere `/opt/noisyless/docker-compose.mqtt-ingest.yml` (overlay)
5. Patche `docker-compose.yml` pour:
   - Fixer le double-mount des volumes mosquitto
   - Ajouter le port 1883 au mapping
6. `docker compose up -d --build mosquitto mqtt-ingest`

### 4.2 Permissions Mosquitto

Probleme rencontre : le passwd cree par defaut etait owned root mode 600, Mosquitto (UID 1883) ne pouvait pas le lire. Solution :
```bash
docker run --rm -v /opt/noisyless/mosquitto/config:/mosquitto/config --user root \
    eclipse-mosquitto:2 \
    sh -c 'rm -f /mosquitto/config/passwd && \
           mosquitto_passwd -b -c /mosquitto/config/passwd esp32 "<pass>" && \
           chown 1883:1883 /mosquitto/config/passwd && \
           chmod 644 /mosquitto/config/passwd'
```

### 4.3 Reseau Docker

Mosquitto et mqtt-ingest sont sur le reseau `noisyless_default` (auto-cree par compose). DNS interne resout `mosquitto`, `timescaledb`, `api`, etc.

---

## 5. Comment flasher l'ESP32

### 5.1 Prerequis

- PlatformIO installe : `pip install platformio` (ou venv)
- ESP32-S3 connecte en USB (apparait comme `/dev/ttyACM0`)
- User dans groupe `dialout`

### 5.2 Compilation

```bash
cd /home/mathieu/noisyless/05_FIRMWARE
source /home/mathieu/.pio-venv/bin/activate
pio run -e esp32dev
```

**Premier build** : ~10 min (telecharge toolchain RISC-V, framework Arduino ESP32, esptool)
**Builds suivants** : ~30s

### 5.3 Flash

```bash
pio run -e esp32dev -t upload --upload-port /dev/ttyACM0
```

L'ESP32 reboote automatiquement. Le firmware se connecte au WiFi puis au MQTT.

### 5.4 Lecture des logs serie

```bash
pio device monitor -p /dev/ttyACM0 -b 115200
# OU script Python pour lecture filtree :
python3 -c "
import serial; s = serial.Serial('/dev/ttyACM0', 115200)
while True: print(s.readline().decode(errors='replace').strip())
"
```

### 5.5 Reset programme via DTR/RTS

Pour relancer le boot sans debrancher :
```python
import serial, time
s = serial.Serial('/dev/ttyACM0', 115200)
s.setDTR(False); s.setRTS(True); time.sleep(0.2); s.setRTS(False)
```

---

## 6. Verifications end-to-end

### 6.1 Capteur emet bien des frames LD2410

```python
# Lit les bytes bruts UART pendant 5s
import serial, time
s = serial.Serial('/dev/ttyACM0', 115200)
# (apres reset, lire les logs DIAG si compile en mode diag)
```

Frame valide :
```
F4 F3 F2 F1 0D 00 02 AA <state> <md_l> <md_h> <me> <sd_l> <sd_h> <se> <dd_l> <dd_h> 55 <ck> F8 F7 F6 F5
```

### 6.2 MQTT recoit les messages

Sur le VPS :
```bash
ssh noisyless@91.99.26.43
docker exec -it noisyless-mosquitto-1 mosquitto_sub \
    -h localhost -p 1883 -u esp32 -P '<pass>' \
    -t 'esp32/noisyless/+/datas' -v
```

### 6.3 Ingest insere en DB

```bash
docker logs noisyless-mqtt-ingest --tail 20
```

Doit afficher des lignes `[INGEST] NL-001 | temp=... iaq=... presence=... db_avg=... lux1=...`.

### 6.4 DB contient les mesures

```bash
docker exec noisyless-timescaledb-1 psql -U noisyless -d noisyless -c \
    "SELECT time, device_id, temp, presence, distance_cm, db_avg, lux1 \
     FROM measurements ORDER BY time DESC LIMIT 5;"
```

### 6.5 Dashboard affiche les donnees

`https://platform.noisyless.com/dashboard.html` apres login magic-link.
La carte du capteur doit afficher : Bruit dB, Presence OUI/NON, Temp, Humidite, IAQ, Lumiere.

---

## 7. Limitations actuelles (a corriger pour la vente)

### 7.1 WiFi hardcode
SSID/pass dans le code -> impossible pour client de configurer.
**TODO** : integrer `WiFiManager` (lib deja dans platformio.ini, captive portal AP).

### 7.2 Device ID fixe `NL-001`
Tous les capteurs flashes auront le meme ID -> conflit MQTT topic.
**TODO** : generer ID a partir de la MAC address au boot.

### 7.3 LEDs status non utilisees
Les 3 LEDs RGB sont initialisees mais pas pilotees selon l'etat.
**TODO** : rouge=pas WiFi, bleu=connect MQTT, vert=OK, vert clignotant=OTA.

### 7.4 Pas de TLS MQTT
Port 1883 sans TLS. Mot de passe en clair sur le reseau.
**TODO** : Phase 2, activer 8883 avec certs Let's Encrypt embarques.

### 7.5 OTA via GitHub manifest
Existe deja, pointe sur `mberthod/202411_Projet_029___EnvironnementalSensor/noisyless_ws/ota/stable.json` mais le repo a change pour `mberthod/noisyless`.
**TODO** : creer `ota/stable.json` dans le nouveau repo + GitHub Action de build.

### 7.6 Pas de webhook Stripe
Achat -> aucune creation auto de compte / device.
**TODO** : webhook `/api/stripe/webhook` qui cree le user et envoie le magic-link.

---

## 8. Fichiers cles

```
/home/mathieu/noisyless/
├── 05_FIRMWARE/
│   ├── platformio.ini
│   ├── include/credentials.h          (MQTT_HOST=91.99.26.43, user/pass)
│   └── src/main.cpp                   (parser LD2410 custom integre)
├── deploy/
│   ├── mqtt-ingest/
│   │   ├── mqtt_ingest.py             (pont MQTT -> TimescaleDB)
│   │   ├── Dockerfile
│   │   └── requirements.txt
│   ├── mosquitto/mosquitto.conf
│   └── deploy-vps.sh                  (script idempotent)
├── docs/
│   └── ARCHITECTURE.md                (ce fichier)
└── PLAN_J5_VENTE.md                   (roadmap J+5 pour la vente)
```

```
VPS noisyless@91.99.26.43:/opt/noisyless/
├── docker-compose.yml                 (modifie : mosquitto port 1883 + volumes fixes)
├── docker-compose.mqtt-ingest.yml     (overlay mqtt-ingest)
├── mqtt-ingest/                       (deploye depuis local)
├── mosquitto/config/{mosquitto.conf,passwd}
└── static/dashboard.html              (corrige format imbrique 2026-05-08)
```

---

## 9. Auth / secrets (NE PAS COMMIT)

| Service | Secret | Stockage |
|---------|--------|----------|
| MQTT | user `esp32` / pass | `credentials.h` (.gitignore), `/opt/noisyless/mosquitto/config/passwd` |
| Postgres | user `noisyless` / pass | `/opt/noisyless/.env` |
| JWT | HS256 secret | `/opt/noisyless/.env` |
| SMTP OVH | user/pass | `/opt/noisyless/.env` |
| Stripe | sk_live | `/opt/noisyless/.env` |

`.env` du VPS contient les vrais secrets, ne sort pas du serveur.

---

*-- Artemis, 2026-05-08*
