# NOISYLESS — PLAN DE DÉVELOPPEMENT BACKEND

## ANALYSE DE L'EXISTANT

### Firmware ESP32 (192.168.0.176)
**Statut** : ✅ MQTT configuré correctement
- `MQTT_HOST = "91.99.26.43"` → VPS Hetzner ✅
- `MQTT_PORT = 1883` ✅
- Topic : `noisyless/+/heatmap`
- Capteurs : BME680 (BSEC2), LD2410 (radar), LUX, MIC
- OTA : GitHub `mberthod/noisyless_ws/ota/stable.json`

**Points de vigilance :**
- WiFi en dur : `TP-Link_B150` / `12902637` → à mettre en AP config
- DEVICE_ID : `NL-001` → à rendre unique (MAC address)
- Pas de TLS sur MQTT (port 1883 non sécurisé)

### Server Hetzner (91.99.26.43)
**Containers running :**
```
noisyless-mosquitto      → MQTT broker (1883, 8883)
noisyless-mqtt-ingest    → Python: MQTT → TimescaleDB
noisyless-api            → FastAPI (shop, pilot, customer)
noisyless-timescaledb    → PostgreSQL + Timescale
noisyless-alert-engine   → Python: alertes
noisyless-heatmap-agg    → Python: agrégation heatmap
noisyless-nginx          → Reverse proxy + static
```

**Services API existants :**
- `POST /shop/checkout` → Stripe
- `POST /api/pilot-request` → Formulaire pilote
- `GET /api/customer/check` → Vérif client Stripe
- `GET /health` → Healthcheck

---

## ARCHITECTURE PROPOSÉE

### DEUX DASHBOARDS DISTINCTS

```
┌─────────────────────────────────────────────────────────────┐
│                    NOISYLESS PLATFORM                        │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────────────────┐    ┌─────────────────────┐        │
│  │   ADMIN DASHBOARD   │    │  CLIENT DASHBOARD   │        │
│  │   (mathieu only)    │    │   (clients)         │        │
│  ├─────────────────────┤    ├─────────────────────┤        │
│  │ • Vue tous capteurs │    │ • Vue MES capteurs  │        │
│  │ • Données brutes    │    │ • Historique        │        │
│  │ • Logs MQTT         │    │ • Alertes           │        │
│  │ • Gestion firmware  │    │ • Settings          │        │
│  │ • Users/Clientes    │    │ • Factures          │        │
│  │ • Système           │    │ • Support           │        │
│  └─────────────────────┘    └─────────────────────┘        │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              BACKEND UNIFIÉ (FastAPI)                │   │
│  ├─────────────────────────────────────────────────────┤   │
│  │ /api/admin/*      → Admin only (JWT admin)          │   │
│  │ /api/customer/*   → Client only (JWT customer)      │   │
│  │ /api/public/*     → Open (health, docs)             │   │
│  │ /shop/*           → Stripe checkout                 │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              TIMESCALEDB (PostgreSQL)                │   │
│  ├─────────────────────────────────────────────────────┤   │
│  │ hypertable: sensor_data                             │   │
│  │ - time (TIMESTAMPTZ)                                │   │
│  │ - device_id (TEXT)                                  │   │
│  │ - villa_id (TEXT)                                   │   │
│  │ - db (FLOAT)                                        │   │
│  │ - co2 (FLOAT)                                       │   │
│  │ - temp (FLOAT)                                      │   │
│  │ - hum (FLOAT)                                       │   │
│  │ - presence (BOOL)                                   │   │
│  │ - distance_cm (INTEGER)                             │   │
│  │ - iaq (FLOAT)                                       │   │
│  │                                                     │   │
│  │ hypertable: heatmap_data                            │   │
│  │ - time, device_id, villa_id, grid_json              │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                 MOSQUITTO (MQTT)                     │   │
│  ├─────────────────────────────────────────────────────┤   │
│  │ noisyless/{device_id}/telemetry → ingest            │   │
│  │ noisyless/{device_id}/heatmap   → heatmap-agg       │   │
│  │ noisyless/{device_id}/command   ← device            │   │
│  │ noisyless/{device_id}/ota       ← device            │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

## 1. ADMIN DASHBOARD (`/admin/`)

### Routes
```
GET  /admin/              → Dashboard overview (stats, alerts)
GET  /admin/devices       → Liste tous les capteurs (+ filtres)
GET  /admin/devices/{id}  → Détail + données temps réel
GET  /admin/devices/{id}/data → Historique (graphiques)
GET  /admin/mqtt-logs     → Logs MQTT en temps réel (WebSocket)
GET  /admin/users         → Liste clients + abonnements
POST /admin/users/{id}/suspend → Suspendre compte
GET  /admin/firmware      → Versions + upload OTA
POST /admin/firmware      → Upload nouveau firmware
POST /admin/devices/{id}/ota → Push OTA vers device
GET  /admin/system        → Santé services (DB, MQTT, API)
```

### Tech Stack
- **Frontend** : Vue.js 3 + Vite (même DA : Space Mono + IBM Plex)
- **Charts** : Chart.js ou Apache ECharts (time-series)
- **Real-time** : WebSocket pour logs MQTT + données live
- **Auth** : JWT avec rôle `admin`

### Fichiers à créer
```
/home/mathieu/Kdrive/01_CLIENTS/NOISYLESS/admin-dashboard/
├── src/
│   ├── views/
│   │   ├── Dashboard.vue
│   │   ├── DevicesList.vue
│   │   ├── DeviceDetail.vue
│   │   ├── MqttLogs.vue
│   │   ├── UsersList.vue
│   │   ├── FirmwareManager.vue
│   │   └── SystemHealth.vue
│   ├── components/
│   │   ├── SensorChart.vue
│   │   ├── DeviceCard.vue
│   │   ├── AlertBanner.vue
│   │   └── OtaUploader.vue
│   ├── api/
│   │   └── admin.js
│   └── App.vue
├── package.json
└── vite.config.js
```

---

## 2. CLIENT DASHBOARD (`platform.noisyless.com`)

### Routes
```
GET  /dashboard/          → Vue d'ensemble (mes capteurs)
GET  /dashboard/device/{id} → Détail capteur + historique
GET  /dashboard/alerts    → Historique alertes
GET  /dashboard/settings  → Config (WiFi, seuils, notifications)
GET  /dashboard/billing   → Factures + abonnement
GET  /dashboard/support   → Tickets support
```

### Tech Stack
- **Frontend** : Vue.js 3 + Vite (existant à refactoriser)
- **Auth** : JWT avec rôle `customer`
- **Data** : API `/api/customer/devices`, `/api/customer/data`

### Fichiers à créer/modifier
```
/home/mathieu/Kdrive/01_CLIENTS/NOISYLESS/frontend/
├── src/
│   ├── views/
│   │   ├── Dashboard.vue (refactor)
│   │   ├── DeviceView.vue
│   │   ├── AlertsView.vue
│   │   ├── SettingsView.vue
│   │   ├── BillingView.vue
│   │   └── SupportView.vue
│   ├── components/
│   │   ├── DeviceList.vue
│   │   ├── SensorDisplay.vue
│   │   ├── AlertList.vue
│   │   └── BillingHistory.vue
│   ├── api/
│   │   └── customer.js
│   └── App.vue
├── package.json
└── vite.config.js
```

---

## 3. BACKEND API — NOUVEAUX ENDPOINTS

### Admin Endpoints (`/api/admin/`)
```python
# backend/api/admin.py

GET  /api/admin/devices           → Liste tous devices + status
GET  /api/admin/devices/{id}      → Détail device
GET  /api/admin/devices/{id}/data → Données brutes (Timescale)
GET  /api/admin/mqtt-logs         → WebSocket: logs MQTT live
GET  /api/admin/users             → Liste clients + subscriptions
POST /api/admin/users/{id}/suspend → Suspendre
GET  /api/admin/firmware          → Liste versions OTA
POST /api/admin/firmware          → Upload firmware (.bin)
POST /api/admin/devices/{id}/ota  → Trigger OTA push
GET  /api/admin/system            → Health: DB, MQTT, disk, RAM
```

### Customer Endpoints (`/api/customer/`)
```python
# backend/api/customer.py (extend)

GET  /api/customer/devices        → Liste MES devices
GET  /api/customer/devices/{id}   → Détail
GET  /api/customer/devices/{id}/data → Mes données (7j gratuit, + avec abonnement)
GET  /api/customer/alerts         → Mes alertes
PUT  /api/customer/devices/{id}/settings → Config (seuils, WiFi)
GET  /api/customer/billing        → Mes factures
GET  /api/customer/subscription   → Mon abonnement actuel
POST /api/customer/subscription/upgrade → Upgrade plan
```

### OTA Service (`/api/ota/`)
```python
# backend/api/ota.py

GET  /api/ota/manifest            → JSON: versions disponibles
GET  /api/ota/firmware/{version}.bin → Download .bin
POST /api/ota/push/{device_id}    → Trigger OTA vers device (MQTT command)
```

---

## 4. FIRMWARE UPDATE MANAGEMENT

### OTA Flow
```
1. Admin upload .bin → /api/admin/firmware
2. API stocke dans S3/Kdrive + met à jour manifest
3. Device check toutes les 1h: GET OTA_MANIFEST_URL
4. Si version > current → download + flash
5. Device reboot + report version via MQTT
```

### Manifest JSON (`ota/stable.json`)
```json
{
  "version": "1.1.0",
  "min_version": "1.0.0",
  "url": "https://github.com/mberthod/noisyless/releases/download/v1.1.0/noisyless.bin",
  "changelog": "Fix MQTT reconnect, add heatmap mode",
  "hash": "sha256:abc123..."
}
```

### Firmware Changes Required
```cpp
// main.cpp modifications

// 1. Unique device ID from MAC
String getDeviceId() {
  uint64_t mac = ESP.getEfuseMac();
  return String("NL-") + String(mac, HEX).toUpperCase();
}

// 2. WiFi Manager for config (first boot AP mode)
#include <WiFiManager.h>
WiFiManager wm;
// Auto AP si pas de WiFi config

// 3. MQTT with TLS (optionnel mais recommandé)
// Port 8883 avec certificat
```

---

## 5. MISE À JOUR FIRMWARE — COMMANDES

### Build + Upload OTA
```bash
# 1. Build firmware
cd /home/mathieu/noisyless/05_FIRMWARE
pio run -e esp32dev

# 2. Generate hash
sha256sum .pio/build/esp32dev/firmware.bin > firmware.bin.sha256

# 3. Upload to GitHub releases
gh release create v1.1.0 \
  .pio/build/esp32dev/firmware.bin \
  --title "v1.1.0" \
  --notes "Fix MQTT reconnect"

# 4. Update manifest
# Edit ota/stable.json with new version + hash
git add ota/stable.json
git commit -m "OTA manifest v1.1.0"
git push origin noisyless_ws
```

### Push OTA vers device spécifique
```bash
# Via API admin
curl -X POST https://platform.noisyless.com/api/admin/devices/NL-001/ota \
  -H "Authorization: Bearer ADMIN_JWT" \
  -d '{"version": "1.1.0"}'

# Device reçoit sur MQTT: noisyless/NL-001/ota
# Payload: {"version": "1.1.0", "url": "..."}
```

---

## 6. PLAN D'IMPLÉMENTATION

### Phase 1 — Backend API (3-4 jours)
- [ ] `admin.py` : endpoints devices, users, firmware
- [ ] `customer.py` : endpoints devices, data, alerts, billing
- [ ] `ota.py` : manifest, upload, push
- [ ] JWT auth avec rôles (admin/customer)
- [ ] WebSocket pour MQTT logs

### Phase 2 — Admin Dashboard (4-5 jours)
- [ ] Setup Vue.js + Vite
- [ ] Dashboard overview (stats)
- [ ] Devices list + detail
- [ ] MQTT logs (WebSocket)
- [ ] Firmware manager (upload + push)
- [ ] System health

### Phase 3 — Client Dashboard (3-4 jours)
- [ ] Refactor frontend existant
- [ ] Device view + charts
- [ ] Alerts history
- [ ] Settings (WiFi, thresholds)
- [ ] Billing + subscription

### Phase 4 — Firmware Update (2 jours)
- [ ] WiFi Manager (AP config mode)
- [ ] Unique device ID (MAC)
- [ ] OTA client (check manifest + download)
- [ ] MQTT TLS (optionnel)

---

## 7. STRUCTURE DE FICHIERS FINALE

```
/home/mathieu/Kdrive/01_CLIENTS/NOISYLESS/
├── backend/
│   └── api/
│       ├── main.py           # FastAPI app
│       ├── admin.py          # Admin endpoints
│       ├── customer.py       # Customer endpoints
│       ├── ota.py            # OTA management
│       ├── shop.py           # Stripe checkout
│       ├── pilot.py          # Pilot requests
│       ├── auth.py           # JWT auth
│       └── devices.py        # Device CRUD
├── admin-dashboard/          # Vue.js admin
│   └── src/
├── frontend/                 # Vue.js client
│   └── src/
├── services/
│   ├── mqtt-ingest/          # MQTT → TimescaleDB
│   ├── alert-engine/         # Alert processing
│   └── heatmap-agg/          # Heatmap aggregation
├── 05_FIRMWARE/              # ESP32 firmware
│   ├── src/main.cpp
│   ├── include/
│   └── platformio.ini
└── ota/
    └── stable.json           # OTA manifest
```

---

## 8. PROCHAINES ACTIONS IMMÉDIATES

1. **Créer `backend/api/admin.py`** — Endpoints admin de base
2. **Créer `backend/api/ota.py`** — Gestion OTA
3. **Setup Vue.js admin** — `admin-dashboard/`
4. **Firmware WiFi Manager** — Permettre config sans reflash

**Tu veux que je commence par quoi ?**
- A. Backend API (admin.py, ota.py)
- B. Admin Dashboard (Vue.js)
- C. Firmware WiFi Manager + OTA client
- D. Autre priorité
