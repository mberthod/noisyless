# Session du 2026-05-07 / 2026-05-08

## Resume

Premiere mise en service end-to-end : ESP32 physique -> MQTT VPS -> DB -> dashboard live.

## Realisations chronologiques

### 1. Audit (2026-05-07 17h)
- Lecture complete de `05_FIRMWARE/src/main.cpp` (907 lignes)
- Lecture de `noisyless-platform-mvp/` (FastAPI + TimescaleDB + Vue3)
- Lecture de `HANDOFF.md` (etat du dashboard)
- Identification des 5 gaps bloquants vente (WiFi hardcode, MQTT broker incorrect, pas d'ingest, mosquitto down, conversion mic manquante)
- Document : `PLAN_J5_VENTE.md`

### 2. Service mqtt_ingest (2026-05-07 17h-18h)
- Code Python ~150 lignes (paho-mqtt + psycopg2)
- Conversion ADC mic -> dB SPL (formule log)
- Conversion ADC lux -> lux (lineaire 0.25)
- Reconnexion auto MQTT + DB
- Container Docker avec `restart: unless-stopped`
- Document : `deploy/mqtt-ingest/mqtt_ingest.py`

### 3. Deploiement VPS Hetzner (2026-05-07 18h)
- Script `deploy-vps.sh` idempotent
- Fix volumes mosquitto (double-mount config + certs)
- Ajout port 1883 expose
- Creation passwd avec UID 1883 (mosquitto user)
- Ajout `firmware_version` colonne dans table `devices`

### 4. Test MQTT simule + DB (2026-05-07 19h)
- `mosquitto_pub` test message -> ingest -> insert DB OK
- Pipeline valide AVANT branchement firmware reel

### 5. Compilation firmware (2026-05-08 08h-09h)
- Installation PlatformIO en venv Python (`/home/mathieu/.pio-venv/`)
- Premier build : 10 min (toolchain RISC-V + framework Arduino + esptool)
- Output : `firmware.bin` 976 KB
- Modification `credentials.h` pour pointer MQTT vers 91.99.26.43

### 6. Premier flash + connexion live (2026-05-08 09h)
- ESP32 detecte sur `/dev/ttyACM0`
- Flash 6 sec
- WiFi `TP-Link_B150` connecte (RSSI -47 dBm)
- BSEC2 init OK : temp 24.49C, hum 47%, IAQ 50, CO2 500ppm
- MQTT connecte au VPS
- Donnees inserees toutes les 10s -> dashboard

### 7. Probleme LD2410 (2026-05-08 13h)
- LD2410 d'origine : capteur defectueux (pas de LED rouge)
- Mathieu reconnecte un autre capteur fonctionnel
- Mais la lib `ncmreynolds/ld2410@^0.1.4` echoue toujours `radar.begin()`
- Test bauds 256000 / 9600 : aucune reponse

### 8. Diagnostic UART brut (2026-05-08 13h)
- Ajout fonction `diagRadarRaw()` qui dump les bytes bruts
- Reset ESP32 via DTR/RTS pour capturer les logs DIAG au boot
- DECOUVERTE : le LD2410 envoie bien des frames a 256000 baud (`F4 F3 F2 F1 ... F8 F7 F6 F5`)
- La lib etait buggee, pas le hardware

### 9. Parser LD2410 custom (2026-05-08 13h)
- Implementation `parseLD2410Stream()` ~50 lignes
- Base sur datasheet HLK-LD2410 V1.02 fournie par Mathieu
- Parse type frame `02 AA` (target data) avec state, distances moving/stationary, energies, detection_dist
- Bypass complet de la lib
- Re-flash, presence detectee : distance 30-115 cm selon mouvement

### 10. Bug dashboard (2026-05-08 13h)
- Symptome : seul `lux` s'affichait, autres champs vides
- Cause : `/api/devices/{id}/live` retourne format imbrique (`bme.temp`, `ld2410.presence`, `mic.db_avg`) mais dashboard.html lisait en plat (`d.live.temp`, etc.)
- Fix : remplacement par `d.live.bme?.temp`, `d.live.ld2410?.presence`, `d.live.mic?.db_avg`
- Upload + nginx reload

### 11. Documentation (2026-05-08 13h30)
- `docs/ARCHITECTURE.md` (ce repo)
- `docs/CHANGELOG-2026-05-08.md` (ce fichier)

## Donnees live actuellement (2026-05-08 13h)

Device : NL-001 (MAC B43A45B7A9B0)
Mesures toutes les 10s :
- Temp 27.1-27.5°C
- Humidite 43-47%
- IAQ 50 (stabilisation)
- CO2 500 ppm
- Pression 95867 Pa (cohérent altitude)
- Presence : detecte dynamiquement avec distance 30-150 cm
- Bruit 74-82 dB (selon environnement)
- Lumiere : variable selon eclairage piece

## Fix permanents appliques

1. `claw-postgres` (autre infra MBHHOLD) : `restart=unless-stopped` (etait `no`)
2. `claw-postgres` reconnecte au reseau Docker avec alias DNS
3. Mosquitto compose : volumes corriges (separer config et certs)
4. Mosquitto config : listener 1883 avec auth password_file
5. mqtt_ingest : `ON CONFLICT DO NOTHING` sur measurements (idempotent)
6. mqtt_ingest : auto-reconnect DB si OperationalError
7. Firmware : parser LD2410 custom remplace lib

## Restant pour J+5 (vente)

Voir `PLAN_J5_VENTE.md` section "PLAN J+5".
- WiFiManager (provisioning client)
- Device ID auto (MAC)
- LED status
- Webhook Stripe -> creation compte
- Tunnel acquisition sur noisyless.com (CTAs Stripe)
- OTA GitHub Actions
