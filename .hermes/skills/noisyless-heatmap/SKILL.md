---
name: noisyless-heatmap
description: "Développement firmware/hardware NOISYLESS HeatMap #091 — node ToF, node Radar, mesh ESP-NOW, plateforme"
version: 1.0.0
category: iot-firmware
metadata:
  hermes:
    tags: [noisyless, heatmap, esp32, firmware, kicad, iot]
    project: "#091"
---

# NOISYLESS HeatMap — Règles de développement

## Périmètre
Ce skill couvre le développement firmware/hardware du sous-projet HeatMap (#091) de NOISYLESS : nodes ToF intérieur (TMF8829), nodes Radar extérieur (LD6001A), mesh ESP-NOW, master recalage spatial, platform backend.

## Architecture — règles d'or

### Hardware
- **MCU** : ESP32-S3-MINI-1. Pas d'Arduino — **ESP-IDF uniquement**.
- **ToF intérieur** : TMF8829 (ams-OSRAM), I²C 0x29, mode 16×16, batterie 1×18650.
- **Radar extérieur** : **LD6001A prioritaire** (time-to-market). Fallback IWR6843AOP uniquement si besoin >10 personnes.
- **Alim extérieur** : panneau solaire + batterie **3 jours autonomie sans soleil**.
- **Alim intérieur** : batterie 18650, duty-cycle 60-120s.
- **Pas de gateway** : WiFi mesh ESP-NOW, master élu (bully algorithm).

### Firmware
- **Framework** : ESP-IDF + FreeRTOS. Pas PlatformIO/Arduino pour la prod.
- **Mesh** : ESP-NOW, trames **binaires uniquement** (pas de JSON dans le mesh — limite 250 bytes).
- **Clustering** : connected-components 4-way flood fill sur grille 16×16. **Pas de DBSCAN** — overkill et trop lourd en RAM.
- **Master** : élection bully (MAC la plus basse). Recalage spatial local sur le master ESP32-S3. Le backend reçoit la heatmap déjà fusionnée.
- **Power** : deep sleep RTC, watchdog 10s, ADC batterie via diviseur 1:2.

### Plateforme
- **Frontend** : `https://platform.noisyless.com` — fichiers statiques dans `/opt/noisyless/static/`.
- **API** : FastAPI sur Hetzner CAX11, proxy nginx `/api/` → `api:8000`.
- **Auth** : magic link (JWT 30 min), endpoint `/api/auth/send-magic-link`.
- **DB** : TimescaleDB, tables `devices`, `user_devices`, `provision_tokens`, `floor_plans`, `rooms`, `device_positions`.
- **CORS** : nginx doit servir `Access-Control-Allow-Origin: *` sur `/api/` et `/shop/` pour que le site `noisyless.com` puisse appeler l'API `platform.noisyless.com`. Voir `references/platform-infra.md` pour la config nginx complète.

## Kdrive — structure projet complète

``` 
01_CLIENTS/NOISYLESS/
├── 00_INFOS_CLIENT/
│   ├── Contacts.md                           ← contacts client NOISYLESS
│   └── CAHIER_RECETTE_NOISYLESS.md           ← audit complet état des lieux (25/05/2026)
├── 01_PROJETS/
│   ├── 202604_089_Capteur_Environnemental_V1/  ← #089 capteur bruit/air (ESP32-S3 + BME680 + LD2410)
│   │   ├── 01_CAHIER_DES_CHARGES/
│   │   ├── 02_ETUDE_PRELIMINAIRE/
│   │   ├── 03_CAO/                           ← KiCad (RevB + V01), BOM, positions
│   │   ├── 04_FABRICATION/                   ← boîtiers 3D (.3mf), photos
│   │   └── 05_FIRMWARE/                      ← PlatformIO Arduino+FreeRTOS (main.cpp, BSEC2, LD2410, MQTT)
│   │       └── lib/, src/, include/, .vscode/
│   └── 202605_091_HeatMap_Presence/           ← #091 HeatMap (ce projet)
│       ├── 01_NOISYLESS_SIMPLE/
│       ├── 02_NOISYLESS_HEATMAP_INT/
│       ├── 03_NOISYLESS_HEATMAP_EXT/
│       ├── docs/                             ← RADAR_MS72SF1.md, SPEC_FIRMWARE.md
│       └── hardware/                         ← netlist.csv, SPEC_HARDWARE.md, KiCad
├── _infra/                                    ← infra déployée → /opt/noisyless/ sur Hetzner
│   ├── docker-compose.yml
│   ├── services/ (api, mqtt-ingest, heatmap-agg, alert-engine)
│   ├── mosquitto/config/
│   ├── nginx/ (nginx.conf, ssl/)
│   ├── timescaledb/init/
│   └── static/                               ← fichiers statiques servis par nginx
├── _admin/emails/
└── .git/
```

**Convention** : `YYYYMM_NNN_Description_Courte/` — pas de `Projet_`, pas de `__`, pas de majuscules sauf acronymes.

## Backlog autonome

Le système de backlog autonome gère les tâches NOISYLESS en arrière-plan. Voir `references/backlog-system.md` pour le fonctionnement complet.

## Firmware — structure composants ESP-IDF

```
firmware/node-tof-esp32s3/
├── platformio.ini        ← ESP-IDF framework, ESP32-S3, 240 MHz, deep sleep
├── partitions.csv        ← factory + ota_0 + ota_1, 8 MB flash
├── sdkconfig.defaults    ← optimisation -Os, PM, tickless idle
├── main/
│   ├── main.c           ← sensor_task, health_task, master_task, timers
│   ├── power.c/h        ← battery ADC, deep sleep, watchdog
├── components/
│   ├── tmf8829/         ← driver I²C, init, ranging, read 16×16
│   ├── cluster/         ← flood-fill 4-way, centroids, world coords
│   ├── mesh/            ← ESP-NOW, bully election, heartbeat, peer mgmt
│   └── config/          ← NVS persistence, factory defaults
```

## Power budget — calcul batterie

- ToF actif : 117 mA × 33ms = 3.86 mAs/lecture
- Standby : 65 µA
- ESP-NOW TX : ~80 µAh
- 1×18650 (3000 mAh), cycle 60s → ~380 jours autonomie
- Seuil bas : 3.2V → deep sleep 10 min
- Critique : 3.0V → shutdown

## Pièges / erreurs fréquentes

- ❌ Ne JAMAIS proposer du ToF pour l'extérieur diurne (portée ~60 cm plein soleil).
- ❌ Ne JAMAIS mettre du JSON dans les trames ESP-NOW (dépasse 250 bytes).
- ❌ Ne pas faire de DBSCAN sur l'ESP32 — le connected components sur grille 16×16 suffit.
- ❌ Ne pas utiliser Arduino — ce projet est en ESP-IDF bare metal.
- ❌ Ne pas créer de gateway séparée — le mesh ESP-NOW avec master élu suffit.
- ❌ Ne pas mettre de caméra RGB/optique — RGPD by design.
- ❌ Pour les transferts vers Hetzner — utiliser `rsync`, pas `scp` (timeout).
- ❌ Pour les datasheets TMF8829/LD6001A — les sites fabricants ont des portails de login. Téléchargement manuel requis.
- ❌ **FastAPI — import manquant = crash silencieux** : si un router utilise `HTTPBearer` ou `HTTPAuthorizationCredentials` sans les importer (`from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials`), le conteneur crash en boucle (`Restarting (1)`) avec `NameError: name 'HTTPBearer' is not defined`. Symptôme : `docker ps` montre l'API en restart loop. Fix : `sed` l'import manquant → `docker compose build api` → `docker compose up -d api`.
- ❌ **SSH 22 refusé mais serveur ping OK** : si `ssh` donne `Connection refused` mais que le serveur ping et que les ports 80/443/1883 répondent, c'est sshd qui est down. Le mot de passe root Hetzner n'est pas stocké dans nos fichiers — il faut passer par la console Hetzner Cloud → "Reset root password" pour récupérer l'accès.
- ❌ **`docker compose restart` ne recharge pas les variables d'environnement** : si tu changes une variable dans `.env` (ex: `STRIPE_SECRET_KEY`), `docker compose restart api` ne suffit pas — le conteneur garde l'ancienne valeur. Il faut `docker compose up -d api` pour recreater le conteneur avec les nouvelles env vars. Vérifier avec `docker exec noisyless-api python3 -c "import os; print(os.getenv('STRIPE_SECRET_KEY')[-4:])"`.
- ❌ **Ne pas confondre clé Stripe publique et secrète** : `pk_live_...` = publique (frontend JS uniquement), `sk_live_...` = secrète (backend API). L'endpoint `/shop/checkout` a besoin de la `sk_live_`. La `pk_live_` ne marchera pas et donnera `AuthenticationError: Invalid API Key`.
- ❌ **Ne pas exécuter les tâches manuellement si le cron tourne** : le backlog autonome (cron toutes les 15 min) prend une tâche `pending` à la fois. Si tu exécutes en parallèle, le fichier `todo.json` sera corrompu par des écritures concurrentes. Laisser le cron tourner, ou le pauser (`cronjob action=pause`) avant d'intervenir manuellement.
- ❌ **CORS — `fetch()` cross-origin bloqué par le navigateur** : si le site (noisyless.com) appelle l'API (platform.noisyless.com), le navigateur bloque la requête sans headers CORS. Fix dans nginx : ajouter `add_header Access-Control-Allow-Origin "*"` always;` + `add_header Access-Control-Allow-Methods "GET, POST, OPTIONS" always;` + `add_header Access-Control-Allow-Headers "Content-Type, Authorization" always;` + un bloc `if ($request_method = OPTIONS) { return 204; }` dans chaque `location /api/` et `location /shop/`. **Ne pas utiliser `sed` pour injecter ces headers** — les `sed -i` successifs créent des doublons qui cassent la config. Toujours copier la source propre depuis Kdrive (`_infra/nginx/nginx.conf`), éditer avec `patch`, puis `scp` vers le serveur et `docker compose restart nginx`. Vérifier avec `curl -sI -X OPTIONS -H 'Origin: ...' http://localhost/shop/checkout | grep -i access-control`.
- ❌ **Deux domaines distincts** : `noisyless.com` (site vitrine statique, Cloudflare) ≠ `platform.noisyless.com` (app dashboard, Hetzner nginx). Le site vitrine appelle l'API du dashboard via `fetch('https://platform.noisyless.com/shop/checkout')`. La clé Stripe publique (`pk_live_...`) est dans le JS du site vitrine (`index.html`), la clé secrète (`sk_live_...`) est dans `/opt/noisyless/.env` sur le serveur.
- ❌ **todo.json n'est pas synchronisé avec l'état réel** : le fichier `~/.hermes-mbhrep/backlog/todo.json` est généré depuis `NEWTODO.md` mais ne reflète pas toujours l'état serveur (ex: clés Stripe déjà déployées, tâches marquées `blocked` alors que le prérequis est fait). Toujours vérifier l'état réel sur le serveur (`ssh noisyless@91.99.26.43 "cat /opt/noisyless/.env | grep STRIPE"`, `docker compose logs api`) avant de marquer une tâche comme bloquée.
- ❌ **Cloudflare cache HTML — Purge + vérification** : après un `rsync` de fichiers statiques (HTML/JS/CSS) vers `/opt/noisyless/static/`, Cloudflare peut continuer à servir l'ancienne version même après "Purge Everything" si le cache edge n'a pas expiré. Vérifier avec `curl -s https://noisyless.com/ | grep <pattern_unique>` vs `ssh noisyless@91.99.26.43 "cat /opt/noisyless/static/index.html | grep <pattern>"`. Si différent : (1) dashboard Cloudflare → Caching → Browser Cache TTL à "2 hours", (2) Rules → Page Rules → supprimer toute règle `Cache Level: Cache Everything` sur `noisyless.com/*`, (3) DNS → passer l'enregistrement A en "DNS only" (nuage gris) 2 min pour forcer le hit direct, (4) remettre le nuage orange. Pattern unique à chercher : version hashée du fichier ou string spécifique (`pk_live_` vs `pk_test_`).
- ❌ **mqtt-ingest — callback asyncio dans un thread** : le callback `on_message` de paho-mqtt-client s'exécute dans un thread séparé, pas dans l'event loop asyncio. **Ne jamais appeler `asyncio.create_task()` directement** — ça lève `RuntimeError: no running event loop`. Pattern correct : utiliser une `asyncio.Queue` comme pont thread-safe. Le callback fait `queue.put_nowait((topic, payload))`, et une tâche `message_processor()` lit la queue dans l'event loop principal. Voir `references/mqtt-ingest-pattern.md` pour le code complet.
- ❌ **mqtt-ingest — rebuild Docker requis après modif code** : après avoir modifié `main.py` dans un service Docker, `docker compose restart` ne suffit pas — le conteneur utilise l'ancienne image. **Toujours** : `docker compose build mqtt-ingest && docker compose up -d mqtt-ingest`. Vérifier avec `docker logs noisyless-mqtt-ingest --tail 20`.
- ❌ **OTA ESP32 — manifest JSON structure** : le manifest OTA doit contenir `version`, `min_version`, `url` (GitHub Releases), `changelog`, et `hash` (sha256:...). Le firmware checke toutes les 1h (`OTA_CHECK_INTERVAL_MS = 3600000UL`). Pour forcer le check au boot : appeler `checkOtaManifestAndUpdate()` dans `setup()` après connexion MQTT. Hash à calculer avec `sha256sum firmware.bin`.
- ❌ **PlatformIO sur DGX Spark (192.168.0.176)** : le chemin est `/home/mathieu/.pio-venv/bin/platformio` (pas dans PATH). Board : `esp32-s3-devkitc-1`. Upload : `platformio run -e esp32dev -t upload`. Flash size : 8MB, partitionnement custom (factory + ota_0 + ota_1).
- ❌ **paho-mqtt-client v2 — callback_api_version** : les versions récentes de paho (≥2.0) requièrent `callback_api_version=MQTTv5` dans le constructeur `mqtt.Client()`. Sans ça : `ValueError: Unsupported callback API version`. L'ancienne API (sans paramètre) fonctionne encore mais affiche une `DeprecationWarning`. Pour un service ingest, utiliser l'ancienne API est acceptable — ignorer le warning.
- ❌ **Docker — `restart` ne recharge pas le code** : après avoir modifié un fichier Python dans un service Docker, `docker compose restart service` ne suffit pas — le conteneur utilise l'ancienne image. Il faut `docker compose build service && docker compose up -d service` pour rebuild l'image avec le nouveau code. Pattern : `rsync` le fichier → `ssh serveur "cd /opt/noisyless && docker compose build mqtt-ingest && docker compose up -d mqtt-ingest"`.
- ❌ **TimescaleDB — inspecter le schéma avant INSERT** : la table `heatmap_agg_5min` a un schéma fixe (`window_start, villa_id, zone, total_count, max_count, grid_data`). Avant d'insérer, vérifier avec `docker exec noisyless-timescaledb psql -U noisyless -d noisyless -c "\\d heatmap_agg_5min"`. Adapter le payload JSON pour matcher les colonnes (ex: `grid_json` → `grid_data`, ajouter `zone` par défaut).
- ❌ **API — routes sans préfixe /api/** : certaines routes FastAPI ne sont PAS sous `/api/`. Le health check est à `/health` (pas `/api/health`), l'auth magic link sera probablement à `/auth/send-magic-link`. Le nginx proxy route `/api/` → `api:8000` mais pas `/health` directement. Vérifier `main.py` pour le routage exact avant d'appeler un endpoint. Symptôme typique : `curl https://localhost/api/health` → 404 alors que le conteneur API tourne.
- ❌ **Fichiers locaux vs serveur — pas de source unique** : les fichiers HTML du site vitrine n'ont PAS de copie locale canonique unique. Les fichiers dans `~/.hermes-mbhrep/knowledge/elec-core/projects/NOISYLESS/web/site/` sont partiels (seulement faq.html et sitemap.xml). La source de vérité est `/opt/noisyless/static/` sur le serveur Hetzner. Pour modifier une page : d'abord récupérer la version serveur (`ssh noisyless@91.99.26.43 "cat /opt/noisyless/static/<page>"`), éditer localement, puis rsync vers le serveur.
- ❌ **Pages légales — validation humaine obligatoire** : les pages mentions-legales.html, confidentialite.html et cgv.html sont créées mais ne doivent PAS être modifiées ni publiées sans validation explicite de Mathieu. Contenu juridique = risque. Statut actuel : déployées sur le serveur (200 OK) mais contenu non relu.

## Priorités projet

**Règle absolue** : le hardware (PCB, KiCad, BOM, boîtiers) est géré par Mathieu. Les agents ne doivent PAS créer de tâches hardware. Priorité du backlog : **web > API > infrastructure > firmware**. Le firmware #089 et #091 sont en attente du hardware.

## Déploiement — pattern standard

1. **Local → serveur** : `rsync -avz Kdrive/01_CLIENTS/NOISYLESS/web/site/ noisyless@91.99.26.43:/opt/noisyless/static/` (toujours faire `--dry-run` d'abord)
2. **Rebuild API** : `ssh noisyless@91.99.26.43 "cd /opt/noisyless && docker compose build api && docker compose up -d api"`
3. **Test** : `ssh noisyless@91.99.26.43 "curl -s http://localhost/shop/checkout -X POST -H 'Content-Type: application/json' -d '{...}'"`
4. **Vérification frontend** : `curl -so /dev/null -w '%{http_code}' http://localhost/` → doit retourner 200
5. **Purge cache Cloudflare** : manuel (pas d'API key) → dashboard Cloudflare → Caching → Purge Everything

## Stripe — intégration checkout

- **Endpoint** : `POST /shop/checkout` → reçoit `{items: [{product_id, quantity, subscription_plan}], email, name, company}` → retourne `{session_id, url}`
- **Clé** : `STRIPE_SECRET_KEY=sk_live_...` dans `/opt/noisyless/.env`
- **Test** : `curl -X POST http://localhost/shop/checkout -H 'Content-Type: application/json' -d '{...}'` → doit retourner une URL `https://checkout.stripe.com/c/pay/...`
- **Webhook** : endpoint `/shop/webhook` pour recevoir `checkout.session.completed` → sauvegarde en table `orders` + envoi email confirmation via SMTP OVH (`ssl0.ovh.net:465`)
- **Produit** : `nl-simple` (NOISYLESS Simple, 79€), `nl-map-in`, `nl-map-out`, `nl-leak` (précommandes)
- **Frontend JS** : utiliser `window.location.href = session.url` pour rediriger vers Stripe Checkout. **Ne pas utiliser `stripe.redirectToCheckout({sessionId})`** — l'API renvoie déjà une URL complète. Garder le fallback uniquement pour compatibilité :\n```js\nconst r = await fetch('https://platform.noisyless.com/shop/checkout', {...});\nconst d = await r.json();\nif (d.url) window.location.href = d.url;\nelse if (d.session_id) {\n  const stripe = Stripe('pk_live_...');\n  await stripe.redirectToCheckout({ sessionId: d.session_id });\n}\nelse alert('Erreur: ' + (d.detail || 'inconnue'));\n```\n- **Abonnements** : passer `subscription_plan: "starter" | "pro" | null` dans l'item. Le backend ajoute la ligne d'abonnement récurrent si présent.

## Références internes

- **Site inventory** (pages, endpoints, DB, containers) : `references/site-inventory.md`
- Datasheets : `docs/datasheets/README.md` (URLs de téléchargement)
- PMS Research : `docs/pms/pms-research.md` (Guesty, Hostfully, iCal)
- Spécifications firmware : `docs/SPEC_FIRMWARE.md`
- Spécifications hardware : `hardware/SPEC_HARDWARE.md`
- Plateforme : `platform.noisyless.com`, SSH `noisyless@91.99.26.43`
- **Cahier de recette** : `Kdrive/01_CLIENTS/NOISYLESS/00_INFOS_CLIENT/CAHIER_RECETTE_NOISYLESS.md` (audit complet 25/05/2026)

## Références externes

- Datasheets : `docs/datasheets/README.md` (URLs de téléchargement)
- PMS Research : `docs/pms/pms-research.md` (Guesty, Hostfully, iCal)
- Spécifications firmware : `docs/SPEC_FIRMWARE.md`
- Spécifications hardware : `hardware/SPEC_HARDWARE.md`
- Plateforme : `platform.noisyless.com`, SSH `noisyless@91.99.26.43`
