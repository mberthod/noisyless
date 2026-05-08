# NOISYLESS — Plan J+5 : Systeme pret a vendre

**Date**: 2026-05-07
**Objectif**: Premier capteur vendu et fonctionnel en J+5 (12 mai 2026)
**Temps Mathieu**: 2-3h max (demain J+1), reste = Artemis autonome

---

## AUDIT — Etat actuel

### Firmware (ESP32-S3, PlatformIO)
- **Board**: ESP32-S3-DevKitC-1, flash 8MB, partitions OTA
- **Capteurs**: BME680 (BSEC2: temp, hum, pression, gaz, IAQ, CO2eq, VOC), LD2410 (radar presence/distance), 2x lux analogiques, 2x microphones analogiques (pic-a-pic)
- **Comm**: WiFi + MQTT (PubSubClient) → JSON toutes les 10s
- **Topic MQTT**: `esp32/noisyless/<MAC>/datas`
- **Broker MQTT**: `85.215.145.44:1883` (user: esp32) ← **PROBLEME: ce n'est PAS le VPS prod (91.99.26.43)**
- **OTA**: via manifest GitHub (canal stable), SHA256 verifie
- **WiFi**: SSID/pass **hardcodes** dans le code (TP-Link_B150) ← **BLOQUANT pour client**
- **WiFiManager**: lib presente dans platformio.ini mais **pas utilisee** dans main.cpp

### Plateforme (VPS Hetzner 91.99.26.43)
- **API**: FastAPI + psycopg3 + TimescaleDB, auth magic-link email, JWT 12h
- **Dashboard**: Vue3 (static HTML), affiche devices + live + timeline
- **Mosquitto**: container Docker present mais **pas demarre**
- **MQTT Ingest**: **N'EXISTE PAS** — pas de pont MQTT → DB
- **Stripe**: 4 Payment Links crees (Basic 99€, Basic abo 4.99€/m, Premium 149€, Premium abo 7.99€/m)
- **Site marketing noisyless.com**: heberge OVH, **pas les CTAs Stripe**
- **Webhook Stripe**: **aucun** — achat ne cree pas de compte auto

### Schema DB `measurements`
| Champ DB     | Source firmware         | Mapping |
|-------------|------------------------|---------|
| temp        | temperature_c          | direct  |
| humidity    | humidity_pct           | direct  |
| gas_resistance | gas_ohms            | direct  |
| iaq         | iaq (float→int)        | cast    |
| presence    | presence               | direct  |
| distance_cm | target_distance_cm     | direct  |
| db_avg      | sound_mic1_pp          | **A CONVERTIR** (ADC raw → dB) |
| db_peak     | sound_mic2_pp          | **A CONVERTIR** |
| lux1        | lux1_raw               | **A CALIBRER** (ADC raw → lux) |
| lux2        | lux2_raw               | **A CALIBRER** |

---

## GAPS CRITIQUES (bloquent la vente)

1. **WiFi hardcode** → le client ne peut pas configurer son reseau
2. **Broker MQTT pointe vers mauvais serveur** (85.215.145.44 ≠ VPS 91.99.26.43)
3. **Pas de service MQTT ingest** → donnees firmware n'arrivent jamais en DB
4. **Mosquitto pas demarre** sur le VPS
5. **Conversion ADC mic → dB manquante** → dashboard affiche des valeurs brutes
6. **Pas de tunnel acquisition** → client ne peut pas acheter + recevoir son capteur configure
7. **device_id hardcode** NL-001 → chaque capteur doit avoir un ID unique

---

## PLAN J+5

### J+0 (aujourd'hui 7 mai) — Artemis seul, prep
**Duree Mathieu: 0h**

- [x] Audit firmware + plateforme (ce document)
- [ ] **ARTEMIS**: Creer `mqtt_ingest.py` — service Python qui:
  - Subscribe MQTT `esp32/noisyless/+/datas`
  - Parse JSON firmware
  - Convertit mic_pp → dB approximatif (formule: `20 * log10(pp/4095 * Vref)` + offset calibration)
  - Convertit lux_raw → lux approximatif (lineaire ADC)
  - INSERT dans `measurements` TimescaleDB
  - UPDATE `devices.last_seen`
- [ ] **ARTEMIS**: Creer Dockerfile + ajouter au docker-compose VPS
- [ ] **ARTEMIS**: Preparer config Mosquitto (auth, ports)

### J+1 (8 mai) — Mathieu 2-3h
**Ce qui necessite Mathieu:**

1. **WiFi capteur physique** (15 min):
   - Brancher le NOISYLESS sur ton reseau actuel (partage de connexion ou WiFi local)
   - Me donner le SSID + password pour que je modifie credentials.h temporairement
   - OU: je compile un firmware avec WiFiManager (captive portal) et tu flashe via USB

2. **Deploiement VPS** (30 min de commandes sudo):
   - `scp` les fichiers mqtt_ingest sur le VPS
   - Demarrer Mosquitto + mqtt-ingest containers
   - Verifier que les data arrivent en DB

3. **Validation end-to-end** (30 min):
   - Capteur branche → data visible sur platform.noisyless.com/dashboard
   - Screenshot = preuve que ca marche

4. **DNS optionnel** (10 min):
   - Ajouter un record A `mqtt.noisyless.com` → 91.99.26.43
   - Pour que le firmware pointe vers un hostname (pas une IP)

### J+2 (9 mai) — Artemis seul
- [ ] Integrer WiFiManager dans le firmware (captive portal "NOISYLESS-Setup")
  - Client branche le capteur → WiFi "NOISYLESS-Setup" apparait
  - Se connecte → page de config → entre son SSID/pass
  - Sauvegarde en NVS (flash), reboot, se connecte au WiFi client
- [ ] Device ID unique par capteur (base MAC address)
- [ ] LED status: rouge=pas WiFi, bleu=connecte MQTT, vert=tout OK
- [ ] Deployer firmware OTA manifest pour canal "beta"

### J+3 (10 mai) — Artemis seul
- [ ] Page acquisition noisyless.com:
  - Landing page avec les 4 produits Stripe
  - Formulaire pre-commande (email → magic-link plateforme)
  - Integration Payment Links existants
- [ ] Webhook Stripe basique:
  - A la completion du paiement → creer user + device dans la plateforme
  - Envoyer email "Votre capteur est en cours de preparation"
- [ ] Onboarding premiere connexion (profil user)

### J+4 (11 mai) — Artemis seul
- [ ] Test complet du tunnel: achat Stripe → compte cree → capteur lie → data live
- [ ] Fix bugs trouves
- [ ] Calibration son/lumiere (ajuster formules de conversion)
- [ ] Rate limit sur magic-link (securite)
- [ ] Doc utilisateur minimaliste (1 page: brancher → configurer WiFi → voir dashboard)

### J+5 (12 mai) — GO LIVE
- [ ] Premier capteur pret a expedier
- [ ] Tunnel acquisition fonctionnel
- [ ] Dashboard affiche donnees reelles
- [ ] Prospection: premiers emails/messages aux leads

---

## BESOINS MCP / SKILLS SUPPLEMENTAIRES

### Ce dont j'ai besoin pour etre autonome:

1. **Acces SSH au VPS NOISYLESS** (91.99.26.43)
   - Pour deployer mqtt_ingest, demarrer mosquitto, modifier compose
   - Soit cle SSH, soit commandes sudo a coller

2. **Acces PlatformIO / compilation firmware**
   - Est-ce que PlatformIO est installe sur cette machine?
   - Sinon: `pip install platformio` ou je prepare le firmware.bin et tu flashe

3. **Acces DNS OVH noisyless.com**
   - Pour ajouter `mqtt.noisyless.com` (optionnel, peut utiliser IP directe)

4. **Acces au code source noisyless.com (site marketing OVH)**
   - Pour integrer les CTAs Stripe + page acquisition

### Ce que je peux faire sans toi:
- Ecrire tout le code (mqtt_ingest, firmware WiFiManager, webhook Stripe, landing page)
- Preparer les configs Docker
- Compiler le firmware (si PlatformIO disponible)
- Tester localement

---

## DECISION A PRENDRE (Mathieu)

**Option A** — MVP rapide (J+1 = demain):
- Firmware garde WiFi hardcode (tu configures a la main pour chaque client)
- Broker MQTT = IP directe du VPS
- Pas de WiFiManager, pas de captive portal
- **Avantage**: on valide le flux data end-to-end demain

**Option B** — Produit fini (J+3):
- WiFiManager captive portal
- Device ID auto (MAC)
- LED status
- **Avantage**: experience client pro, mais 2 jours de plus

**Recommandation**: Option A demain pour valider, puis Option B en parallele pour J+3.

---

*-- Artemis, 2026-05-07*
