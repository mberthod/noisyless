---
name: noisyless-deployment
description: Déploiement NOISYLESS — Hetzner, nginx, Stripe, Cloudflare
version: 1.0
created: 2026-05-24
---

# NOISYLESS Deployment

Déploiement du site vitrine et API NOISYLESS sur Hetzner CAX11.

## Architecture

| Élément | Valeur |
|---------|--------|
| **Serveur** | Hetzner CAX11 (91.99.26.43) |
| **SSH** | `noisyless@91.99.26.43` |
| **Code** | `/opt/noisyless/` |
| **Static** | `/opt/noisyless/static/` |
| **Nginx config** | `/opt/noisyless/nginx/nginx.conf` |
| **DNS** | Cloudflare (noisyless.com) |

## Déploiement site vitrine

```bash
# Sync fichiers statiques
rsync -avz /home/mathieu/Kdrive/01_CLIENTS/NOISYLESS/web/site/ noisyless@91.99.26.43:/opt/noisyless/static/ --exclude '*.md'

# Redémarrer nginx
ssh noisyless@91.99.26.43 "cd /opt/noisyless && docker compose restart nginx"
```

## Nginx — Configuration critique

**Problème fréquent** : Les fichiers `.html` statiques (merci.html, annule.html) sont servis par le fallback SPA et retournent `index.html`.

**Solution** : Ajouter une location pour les extensions statiques **avant** le fallback SPA :

```nginx
# Static files - served directly (AVANT le fallback SPA)
location ~* \.(html|css|js|png|jpg|jpeg|gif|ico|svg|woff|woff2)$ {
    root /var/www/static;
    expires 1d;
}

# Root - SPA fallback to index.html (APRÈS)
location / {
    try_files $uri $uri/ /static/index.html;
}
```

**Ordre des blocs `location`** :
1. `/static/` (alias)
2. Extensions statiques (`~* \.(html|css|js|...)$`)
3. `/api/`, `/auth/`, `/shop/` (proxy)
4. `/health`
5. `/` (fallback SPA)

## Stripe Checkout

**Endpoints** :
- `POST /shop/checkout` → Retourne `{session_id, url}`
- `GET /merci.html` → Confirmation commande
- `GET /annule.html` → Paiement annulé

**Clés Stripe** :
- Publique : `pk_live_51TTSCbEvgqhDyN6IaNiWvkC6rlxg2JkKZb6iwXmXKzLTdQsAVFVFBTKJYOCgEBMa2NYN4unCQTUA2fzoy8zTZcyF00mbGKDUEb`
- Secrète : Dans `/opt/noisyless/.env` → `STRIPE_SECRET_KEY=sk_live_...`

**Test** :
```bash
curl -s -X POST https://noisyless.com/shop/checkout \
  -H "Content-Type: application/json" \
  -d '{"items":[{"product_id":"nl-simple","quantity":1}],"email":"test@test.com","name":"Test"}' \
  --insecure | jq '.url'
```

## Cloudflare — DNS et cache

**Vérification DNS** :
```bash
dig noisyless.com +short
# Doit retourner 91.99.26.43 (Hetzner), pas 188.x.x.x (OVH/Cloudflare proxy)
```

**Si le contenu ne se met pas à jour** :
1. Dashboard Cloudflare → DNS → `A noisyless.com`
2. Vérifier que **Content** = `91.99.26.43` (pas OVH)
3. Passer en **DNS only** (nuage gris) temporairement
4. Purger cache : Caching → Configuration → Purge Everything
5. Tester avec `--insecure` :
   ```bash
   curl -s https://noisyless.com/ --insecure | grep pk_live
   ```

**Test bypass Cloudflare** :
```bash
curl -s --resolve noisyless.com:443:91.99.26.43 https://noisyless.com/ --insecure | grep pk_
```

## Backlog autonome

**Fichier** : `~/.hermes-mbhrep/backlog/todo.json`

**Mise à jour** : Après chaque tâche complétée, mettre à jour `status` :
```bash
cat ~/.hermes-mbhrep/backlog/todo.json | jq '(.tasks[] | select(.id == "xxx") | .status) = "done"' > /tmp/todo.json && mv /tmp/todo.json ~/.hermes-mbhrep/backlog/todo.json
```

**Cron** : Job `2a926e79d9ba` (15 min) scanne `NEWTODO.md` et crée des tâches via `delegate_task`.

## Site vitrine — Structure

**Architecture multi-pages** (plus de one-pager) :
```
/opt/noisyless/static/
├── index.html (accueil)
├── produits/
│   ├── noisyless-simple.html
│   └── heatmap.html
├── blog/
│   ├── index.html (liste articles)
│   ├── detecter-fete-airbnb-sans-camera.html
│   ├── rgpd-capteurs-location-conformite.html
│   └── ... (9 articles)
├── faq.html
├── mentions-legales.html
├── confidentialite.html
├── cgv.html
├── pilote.html (formulaire demande pilote)
├── merci.html (confirmation Stripe)
├── annule.html (annulation Stripe)
└── assets/
    ├── logohorizmonochrome.png (logo principal — TOUJOURS celui-ci)
    ├── logo.png
    ├── logo_bicolor.png
    └── logocarre.png
```

**Logo** : Utiliser `<img src="/assets/logohorizmonochrome.png">` dans le header. Jamais de "N" CSS ou emoji.

**Direction artistique** : "L'INSTRUMENT DE PRÉCISION"
- Fond : `#FFFFFF` (light theme, pas dark)
- Encre : `#1A1A1A`
- Signal : `#FF6B00` (ambre, ≤5% surface)
- Polices : `Space Mono` (display) + `IBM Plex Sans` (corps)
- Zéro : dégradé texte, blob radial, emoji, boutons pilule glow

**Logo** : Utiliser `<img src="/assets/logohorizmonochrome.png">` dans le header, pas le "N" CSS.

**Blog — Conversion MD → HTML** :
Les articles `.md` dans `noisyless-deployment/blog/` doivent être convertis en `.html` avant déploiement. Script de conversion :
```python
import os, re
blog_dir = "noisyless-deployment/blog/"
output_dir = "web/site/blog/"
for md_file in os.listdir(blog_dir):
    if md_file.endswith('.md'):
        # Parse frontmatter (title, slug, date, description)
        # Convert markdown headers: # → <h1>, ## → <h2>
        # Convert links: [text](url) → <a href="url">text</a>
        # Wrap in HTML template with DA (Space Mono, light theme)
        output = output_dir + md_file.replace('.md', '.html')
```

**Blog — Liens internes** :
Tous les liens dans les articles doivent avoir `.html` :
- `/blog/[slug].html` (pas `/blog/[slug]`)
- `/produits/[slug].html` (pas `/produits/[slug]`)
- `/faq.html`, `/mentions-legales.html`, etc.

**Vérification systématique des liens** (post-déploiement — OBLIGATOIRE) :
```bash
# Vérifier TOUS les liens internes sur chaque page type
curl -s https://noisyless.com/ | grep -o 'href="/[^"]*"' | grep -v "fonts|assets|cdn"
curl -s https://noisyless.com/blog/ | grep -o 'href="/[^"]*"'
curl -s https://noisyless.com/blog/[article].html | grep -o 'href="/[^"]*"'
curl -s https://noisyless.com/produits/[produit].html | grep -o 'href="/[^"]*"'
```

**Signaux d'erreur et corrections** :
| Erreur | Cause | Correction |
|--------|-------|------------|
| `.html.html` | Double extension (bug script) | `re.sub(r'\.html\.html', '.html', content)` |
| `heatmap-interieur.html` | Ancien nom | Remplacer par `heatmap.html` |
| Liens blog sans `.html` | Script conversion | `re.sub(r'href="/blog/([^"/]+)"', r'href="/blog/\1.html"', content)` |
| Liens produits sans `.html` | Script conversion | `re.sub(r'href="/produits/([^"/]+)"', r'href="/produits/\1.html"', content)` |

**Script de correction complète** (à lancer AVANT rsync) :
```python
import os, re
site_dir = "/home/mathieu/Kdrive/01_CLIENTS/NOISYLESS/web/site/"

def fix_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()
    original = content
    
    # Fix double .html extensions
    content = re.sub(r'\.html\.html', '.html', content)
    
    # Fix blog links
    def fix_blog_link(m):
        slug = m.group(1)
        return f'href="/blog/{slug}"' if slug.endswith('.html') else f'href="/blog/{slug}.html"'
    content = re.sub(r'href="/blog/([^"/]+)"', fix_blog_link, content)
    
    # Fix product links
    def fix_product_link(m):
        slug = m.group(1)
        return f'href="/produits/{slug}"' if slug.endswith('.html') else f'href="/produits/{slug}.html"'
    content = re.sub(r'href="/produits/([^"/]+)"', fix_product_link, content)
    
    # Fix other pages
    for page in ['faq', 'mentions-legales', 'confidentialite', 'cgv', 'pilote']:
        content = re.sub(f'href="/{page}.html.html"', f'href="/{page}.html"', content)
        content = re.sub(f'href="/{page}"', f'href="/{page}.html"', content)
    
    if content != original:
        with open(filepath, 'w') as f:
            f.write(content)
        return True
    return False

for root, dirs, files in os.walk(site_dir):
    for file in files:
        if file.endswith('.html'):
            fix_file(os.path.join(root, file))
```

**Checklist post-déploiement** :
- [ ] `/` → tous les liens fonctionnent
- [ ] `/blog/` → 9 articles listés, liens vers articles OK
- [ ] `/blog/[article].html` → liens vers autres articles + produits OK
- [ ] `/produits/noisyless-simple.html` → liens vers heatmap + FAQ OK
- [ ] `/pilote.html` → formulaire accessible
- [ ] `/faq.html`, `/mentions-legales.html`, etc. → navigation OK

## Dashboard (platform.noisyless.com)

**État actuel** : Le domaine `platform.noisyless.com` sert le site marketing (`index.html`) car le frontend Vue.js n'est pas buildé.

**Pour déployer le dashboard Vue.js** :
1. Build : `cd frontend/ && npm install && npm run build`
2. Déployer `dist/` vers `/opt/noisyless/dashboard/`
3. Configurer nginx avec un `server_name platform.noisyless.com` dédié
4. Ou remplacer `index.html` par le build Vue.js

**Alternative rapide** : Créer un dashboard HTML statique avec la même DA (Space Mono, fond blanc, signal ambre).

## Stripe — Formulaire de commande

**Problème fréquent** : Les radio buttons d'abonnement ne se mettent pas à jour visuellement.

**Solution** : Ajouter des gestionnaires de click sur les labels + fonction `updateVisualSelection()` :
```javascript
document.querySelectorAll('.subscription-option').forEach(option => {
    option.addEventListener('click', function(e) {
        if (e.target.type === 'radio') return;
        const radio = this.querySelector('input[type="radio"]');
        radio.checked = true;
        updateTotal();
        updateVisualSelection();
    });
});

function updateVisualSelection() {
    document.querySelectorAll('.subscription-option').forEach(el => el.classList.remove('selected'));
    const selected = document.querySelector('input[name="subscription"]:checked');
    if (selected) selected.closest('.subscription-option').classList.add('selected');
}
```

**Fallback Stripe** : Si l'API retourne `session_id` sans `url`, utiliser `stripe.redirectToCheckout({ sessionId })`.

## MQTT Ingest Service

**Architecture** :
```
ESP32 (firmware) → MQTT (91.99.26.43:1883) → mosquitto → mqtt-ingest → TimescaleDB
```

**Services** :
- `noisyless-mosquitto` : Broker MQTT (ports 1883, 8883)
- `noisyless-mqtt-ingest` : Python — subscribe `noisyless/+/heatmap`, write DB
- `noisyless-timescaledb` : PostgreSQL + Timescale

**Topics MQTT** :
- `noisyless/{device_id}/heatmap` → Données heatmap (grid)
- `noisyless/{device_id}/telemetry` → Télémétrie (dB, CO₂, temp, hum, presence)
- `noisyless/{device_id}/command` ← Commandes vers device (OTA, reboot)
- `noisyless/{device_id}/ota` ← Notifications OTA

**Tables TimescaleDB** :
```sql
-- Télémétrie principale
SELECT * FROM noisyless.measurements LIMIT 10;
-- Colonnes : time, device_id, db, co2, temp, hum, presence, distance_cm, iaq

-- Heatmap agrégée (5min) — TABLE ACTUELLE
SELECT * FROM noisyless.heatmap_agg_5min LIMIT 10;
-- Colonnes : window_start, villa_id, zone, total_count, max_count, grid_data, created_at

-- Devices enregistrés
SELECT * FROM noisyless.devices;
```

**Problème fréquent — mqtt-ingest silencieux** :
Le service `mqtt-ingest` peut démarrer mais rester silencieux (pas de logs) si :
1. `time.sleep()` utilisé dans contexte `asyncio` → bloque l'event loop
2. Race condition au startup → mosquitto pas encore prêt

**Solution** :
```python
# MAUVAIS (bloque)
async def main():
    await init_db()
    client = connect_with_retry()  # utilise time.sleep()
    client.loop_start()
    while True:
        await asyncio.sleep(60)  # OK

# BON (async-compatible)
async def connect_mqtt_with_retry(max_retries=30, delay=2):
    for attempt in range(max_retries):
        try:
            client = mqtt.Client()
            await asyncio.get_event_loop().run_in_executor(
                None, lambda: client.connect(MQTT_HOST, MQTT_PORT, 60)
            )
            return client
        except Exception as e:
            if attempt < max_retries - 1:
                await asyncio.sleep(delay)  # PAS time.sleep()
            else:
                raise
```

**Problème critique — Callback MQTT dans thread séparé** :
Le callback `on_message()` de Paho MQTT est exécuté dans un thread dédié, PAS dans l'event loop asyncio. Appeler `asyncio.create_task()` directement depuis ce callback lève :
```
RuntimeError: There is no current event loop in thread 'paho-mqtt-client-'
```

**Pattern correct — Queue thread-safe** :
```python
import asyncio
import paho.mqtt.client as mqtt

message_queue: asyncio.Queue = None
running = True

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"[MQTT] ✅ Connected to {MQTT_HOST}:{MQTT_PORT}")
        client.subscribe(MQTT_TOPIC)
        print(f"[MQTT] ✅ Subscribed to {MQTT_TOPIC}")

def on_message(client, userdata, msg):
    print(f"[MQTT] 📩 Message received: {msg.topic}")
    print(f"[MQTT] 📩 Payload: {msg.payload}")
    # Thread-safe : put_nowait dans la queue
    if message_queue:
        message_queue.put_nowait((msg.topic, msg.payload))
        print(f"[MQTT] ✅ Message queued")

def on_disconnect(client, userdata, rc):
    print(f"[MQTT] Disconnected: rc={rc}")

async def message_processor():
    """Tourne dans l'event loop principale"""
    print("[PROCESSOR] Started")
    while running:
        try:
            topic, payload = await asyncio.wait_for(message_queue.get(), timeout=5.0)
            await process_message(topic, payload)
            message_queue.task_done()
        except asyncio.TimeoutError:
            pass
        except Exception as e:
            print(f"[PROCESSOR] Error: {e}")

async def main():
    global message_queue, running
    message_queue = asyncio.Queue()
    
    # Init DB first
    await init_db()
    
    # Start message processor BEFORE connecting MQTT
    asyncio.create_task(message_processor())
    
    # Connect MQTT (in executor)
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    
    loop = asyncio.get_event_loop()
    await loop.run_in_executor(
        None, lambda: client.connect(MQTT_HOST, MQTT_PORT, 60)
    )
    
    print("[MQTT] ✅ Connected, starting loop...")
    client.loop_start()
    
    print("[INIT] ✅ Service running. Waiting for messages...")
    try:
        while running:
            await asyncio.sleep(1)
    except KeyboardInterrupt:
        print("[INIT] Shutting down...")
        running = False
        client.loop_stop()
        client.disconnect()
```

**Test de validation complet** :
```bash
# 1. Publier un message test
docker exec noisyless-mqtt-ingest python3 -c "
import paho.mqtt.client as mqtt, json
c = mqtt.Client()
c.connect('mosquitto', 1883, 60)
c.publish('noisyless/NL-001/heatmap', json.dumps({'villa_id':'test','zone':'salon','total_count':5,'max_count':3}))
c.disconnect()
print('✅ Published')
"

# 2. Vérifier les logs (doit montrer message reçu + DB insert)
docker logs noisyless-mqtt-ingest 2>&1 | tail -15
# Attendu :
# [MQTT] 📩 Message received: noisyless/NL-001/heatmap
# [MQTT] 📩 Payload: b'{"villa_id": "test", ...}'
# [MQTT] ✅ Message queued
# [MQTT] 📩 Decoded: {'villa_id': 'test', ...}
# [DB] ✅ Inserted for NL-001/test/salon

# 3. Vérifier en DB
docker exec noisyless-timescaledb psql -U noisyless -d noisyless -c "
  SELECT window_start, villa_id, zone, total_count, max_count 
  FROM heatmap_agg_5min 
  ORDER BY window_start DESC LIMIT 5;"
```

**Table DB actuelle** : `heatmap_agg_5min` (pas `heatmap_data`)
```sql
-- Structure
\d heatmap_agg_5min
-- window_start | villa_id | zone | total_count | max_count | grid_data | created_at

-- Insert correct
INSERT INTO heatmap_agg_5min (window_start, villa_id, zone, total_count, max_count, grid_data)
VALUES (NOW(), $1, $2, $3, $4, $5);
```

**Test de validation** :
```bash
# 1. Publier un message test
docker exec noisyless-mqtt-ingest python3 -c "
import paho.mqtt.client as mqtt, json
c = mqtt.Client()
c.connect('mosquitto', 1883, 60)
c.publish('noisyless/test/heatmap', json.dumps({'villa_id':'test','zone':'salon','total_count':5}))
c.disconnect()
print('✅ Published')
"

# 2. Vérifier les logs
docker logs noisyless-mqtt-ingest 2>&1 | tail -10
# Doit montrer :
# [MQTT] 📩 Message received: noisyless/test/heatmap
# [MQTT] 📩 Decoded: {'villa_id': 'test', ...}
# [DB] ✅ Inserted for NL-001/test/salon

# 3. Vérifier en DB
docker exec noisyless-timescaledb psql -U noisyless -d noisyless -c \
  "SELECT window_start, villa_id, zone, total_count FROM heatmap_agg_5min ORDER BY window_start DESC LIMIT 1;"
```

**Déploiement mqtt-ingest** :
```bash
# Après modification de main.py
cd /opt/noisyless
docker compose build mqtt-ingest
docker compose up -d mqtt-ingest

# Vérifier
docker logs noisyless-mqtt-ingest 2>&1 | tail -20
docker ps | grep mqtt-ingest
```

**Test MQTT manuel** :
```bash
# Publish test
docker exec noisyless-mosquitto mosquitto_pub -h localhost \
  -t "noisyless/NL-001/heatmap" \
  -m '{"villa_id":"test","db":45}'

# Subscribe test (dans autre terminal)
docker exec noisyless-mosquitto mosquitto_sub -h localhost -t "noisyless/#" -v

# Vérifier service mqtt-ingest
docker logs noisyless-mqtt-ingest --tail 50
docker exec noisyless-mqtt-ingest python -c \
  'import paho.mqtt.client as mqtt; c=mqtt.Client(); c.connect("mosquitto",1883); print("OK"); c.disconnect()'
```

**Debug mqtt-ingest** :
```bash
# Vérifier que le service tourne
docker ps | grep mqtt-ingest
docker stats noisyless-mqtt-ingest --no-stream

# Vérifier connexion DB
docker exec noisyless-mqtt-ingest python -c \
  'import asyncpg; asyncio.run(asyncpg.create_pool(host="timescaledb", user="noisyless", password="noisyless_secret_password", database="noisyless")); print("DB OK")'

# Vérifier réseau Docker
docker network inspect noisyless_noisyless-net --format '{{range .Containers}}{{.Name}}: {{.IPv4Address}}{{"\n"}}{{end}}'
docker exec noisyless-mqtt-ingest python -c 'import socket; print(socket.gethostbyname("mosquitto"))'
```

**Firmware ESP32 — Configuration MQTT** :
```cpp
// /home/mathieu/noisyless/05_FIRMWARE/include/credentials.h
static const char* MQTT_HOST = "91.99.26.43";   // VPS Hetzner
static const uint16_t MQTT_PORT = 1883;
static const char* MQTT_USER = "esp32";
static const char* MQTT_PASS = "am#k^keu5nBDWLB";

// Device ID unique (MAC address)
String getDeviceId() {
  uint64_t mac = ESP.getEfuseMac();
  return String("NL-") + String(mac, HEX).toUpperCase();
}
```

---

## OTA — Firmware Updates

**Architecture** :
```
GitHub Releases (.bin) → Manifest JSON → ESP32 check → Download → Flash
```

**Manifest OTA** (`ota/stable.json`) :
```json
{
  "version": "1.0.0",
  "min_version": "1.0.0",
  "url": "https://github.com/mberthod/noisyless/releases/download/v1.0.0/firmware.bin",
  "changelog": "Initial release — MQTT to VPS, LD2410 parser",
  "hash": "sha256:abc123..."
}
```

**Firmware — Check OTA** :
```cpp
// main.cpp
static const char* OTA_MANIFEST_URL = 
  "https://raw.githubusercontent.com/mberthod/noisyless/main/ota/stable.json";

void checkOTA() {
  HTTPClient http;
  http.begin(OTA_MANIFEST_URL);
  int code = http.GET();
  if (code == 200) {
    // Parse JSON, compare version
    // If newer: download + Update.begin()
  }
}
```

**Build + Deploy OTA** :
```bash
# 1. Build firmware
cd /home/mathieu/noisyless/05_FIRMWARE
pio run -e esp32dev

# 2. Generate hash
sha256sum .pio/build/esp32dev/firmware.bin > firmware.bin.sha256

# 3. Create GitHub release
gh release create v1.0.0 \
  .pio/build/esp32dev/firmware.bin \
  --title "v1.0.0" \
  --notes "Initial release"

# 4. Update manifest
cat > ota/stable.json << 'EOF'
{
  "version": "1.0.0",
  "url": "https://github.com/mberthod/noisyless/releases/download/v1.0.0/firmware.bin",
  "hash": "$(cat firmware.bin.sha256)"
}
EOF
git add ota/stable.json && git push
```

**Problème fréquent — Repo mismatch** :
Le firmware peut pointer vers un ancien repo (`202411_Projet_029___EnvironnementalSensor`) au lieu de `noisyless`.

**Correction** :
```cpp
// À jour
static const char* OTA_MANIFEST_URL = 
  "https://raw.githubusercontent.com/mberthod/noisyless/main/ota/stable.json";
```

---

## Backend API — Endpoints

**Admin** (`/api/admin/*` — JWT requis) :
- `GET /api/admin/devices` — Liste tous devices
- `GET /api/admin/devices/{id}/data` — Données brutes
- `GET /api/admin/mqtt-logs` — WebSocket logs MQTT live
- `POST /api/admin/firmware` — Upload firmware
- `POST /api/admin/devices/{id}/ota` — Push OTA

**Customer** (`/api/customer/*` — JWT client) :
- `GET /api/customer/devices` — Mes devices
- `GET /api/customer/devices/{id}/data` — Mes données
- `GET /api/customer/billing` — Factures
- `POST /api/customer/subscription/upgrade` — Upgrade plan

**OTA** (`/api/ota/*`) :
- `GET /api/ota/manifest` — Manifest JSON
- `GET /api/ota/firmware/{version}.bin` — Download .bin
- `POST /api/ota/push/{device_id}` — Trigger OTA

**Existant** :
- `POST /shop/checkout` — Stripe
- `POST /api/pilot-request` — Formulaire pilote
- `GET /api/customer/check?email=...` — Vérif client Stripe

---

## Références

- `references/nginx-config.md` — Configuration nginx complète
- `references/stripe-flow.md` — Flux Stripe et endpoints
- `references/cloudflare-dns.md` — Gestion DNS et cache
- `references/mqtt-ingest.md` — MQTT ingest service + debug
- `references/ota-workflow.md` — OTA firmware workflow
- `references/backend-api.md` — Endpoints API backend
