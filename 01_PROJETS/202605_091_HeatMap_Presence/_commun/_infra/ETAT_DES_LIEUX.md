# NOISYLESS — État des lieux technique

**Date :** 24 Mai 2026  
**Périmètre :** Firmware + Backend + Infra  
**Objectif :** Audit avant attaque Bloc 1

---

## 📍 Contexte

NOISYLESS est un projet de détection de présence par capteurs ToF (Time of Flight) et radar mmWave, avec heatmap de densité et alertes de suroccupation.

**Deux branches de développement :**
1. `202605_091_HeatMap_Presence` — Projet #091, pilote villa (Kdrive client)
2. `_infra` — Infrastructure serveur déployée (VPS 91.99.26.43)

---

## 1. Firmware — État actuel

### 1.1 Node TOF ESP32-S3 (`node-tof-esp32s3/`)

**Localisation :**
```
/home/mathieu/Kdrive/01_CLIENTS/NOISYLESS/202605_091_HeatMap_Presence/firmware/node-tof-esp32s3/
```

**Stack technique :**
| Élément | Valeur |
|---------|--------|
| MCU | ESP32-S3-MINI-1 (WiFi + BLE) |
| Framework | ESP-IDF (PlatformIO) |
| Capteur | AMS TMF8829 (dToF 8×8 ou 16×16 zones) |
| Flash | 8MB, partition OTA |
| Freq CPU | 240 MHz |

**Architecture firmware :**
```
main.c (app_main)
├── mesh_init()         — ESP-NOW mesh, élection master/slave
├── sensor_task()       — Core 0 : TMF8829 ranging + clustering
├── health_task()       — Core 0 : battery, LEDs, role indicator
└── master_task()       — Core 0 : fusion multi-noeuds → MQTT (TODO)
```

**Rôles mesh :**
- **SLAVE** (défaut) : ToF → cluster → ESP-NOW vers master
- **MASTER** (élu) : collecte tous les noeuds → fusion → MQTT

**Fonctionnalités implémentées :**

| Feature | État | Notes |
|---------|------|-------|
| Init TMF8829 | ✅ | `tmf8829_init()` dans `components/tmf8829/` |
| Ranging 16×16 | ✅ | `tmf8829_start_ranging()` + `tmf8829_read_result()` |
| Clustering 4-way | ✅ | `cluster.c` — algorithme de regroupement |
| ESP-NOW mesh | ✅ | `mesh.c` — envoi/reception payloads |
| Élection master | ✅ | Basé sur MAC address (plus haut = master) |
| Deep sleep | ✅ | `esp_deep_sleep()` avec timer RTC |
| Battery ADC | ✅ | GPIO1, divider 1:2, seuil 3000mV |
| LEDs RGB | ✅ | Rouge/Vert/Bleu — status + role |
| Config NVS | ✅ | `config.c` — identity, calib, sensor, power |
| Calibration | ✅ | `x_m`, `y_m`, `theta_deg` — transformation monde |
| Master fusion | 🟡 | Stockage ring buffer, MQTT **non implémenté** |

**Payload ESP-NOW (slave → master) :**
```c
typedef struct {
    uint8_t  src_mac[6];
    uint16_t battery_mv;
    uint16_t seq_num;
    uint8_t  cluster_count;
    mesh_cluster_t clusters[MAX_CLUSTERS];  // x_cm, y_cm, cells, confidence
} mesh_data_tof_t;
```

**Points bloquants / TODO :**

| # | Problème | Impact | Priorité |
|---|----------|--------|----------|
| F1 | MQTT pas implémenté dans `master_task()` | Bloc P3 backend | 🔴 |
| F2 | Pas de provisioning WiFi/SoftAP | Claim impossible | 🔴 |
| F3 | Pas de TLS MQTT | Sécurité | 🟠 |
| F4 | Pas de NVS chiffré | Sécurité | 🟠 |
| F5 | `master_task()` fusion simpliste (union brute) | Précision heatmap | 🟡 |
| F6 | Pas de gestion canal OTA (stable/beta) | Déploiement | 🟡 |
| F7 | Calibration manuelle (NVS) | UX déploiement | 🟡 |

**Fichiers composants (à vérifier) :**
```
components/
├── tmf8829/     — Driver capteur ToF
├── cluster/     — Algorithme clustering 4-way
├── mesh/        — ESP-NOW mesh + élection
└── config/      — NVS config management
```

---

### 1.2 Node Radar (`node-radar-60ghz/`)

**Localisation :**
```
/home/mathieu/Kdrive/01_CLIENTS/NOISYLESS/202605_091_HeatMap_Presence/firmware/node-radar-60ghz/
```

**État :** **Vide** (répertoire existe, aucun fichier)

**Cible hardware (spécification) :**
| Élément | Valeur |
|---------|--------|
| Capteur | TI IWR6843AOP ou IWRL6432 (60 GHz) |
| MCU | ESP32-S3 ou TI SoC |
| Portée | Jusqu'à 30 m extérieur |
| Payload | `{targets: [{x, y, z, v}], count, ts}` |

**Travail restant :**
- Choix capteur (IWR6843AOP = perf, IWRL6432 = basse conso)
- SDK TI mmWave + pont UART ESP32
- Firmware ESP-IDF ou TI CCS
- Intégration mesh ESP-NOW (si ESP32)

---

## 2. Backend — État actuel

### 2.1 Services Docker (`_infra/services/`)

**Localisation :**
```
/home/mathieu/Kdrive/01_CLIENTS/NOISYLESS/_infra/services/
```

| Service | Fichiers | Rôle | État |
|---------|----------|------|------|
| `mqtt-ingest` | `main.py`, `Dockerfile`, `requirements.txt` | Subscribe MQTT → TimescaleDB | ✅ Fonctionnel |
| `heatmap-agg` | `main.py`, `Dockerfile`, `requirements.txt` | Agrège 5min → heatmap | ✅ Fonctionnel |
| `alert-engine` | `main.py`, `Dockerfile`, `requirements.txt` | Seuils → Telegram | ✅ Fonctionnel |
| `api` | `main.py`, `Dockerfile`, `requirements.txt` | FastAPI REST | ✅ Fonctionnel |

---

### 2.2 mqtt-ingest

**Code :** `_infra/services/mqtt-ingest/main.py`

**Fonctionnement :**
```python
subscribe("noisyless/+/heatmap")
  ↓
parse JSON payload
  ↓
extract device_id, villa_id, total_count, heatmap, battery_mv
  ↓
INSERT INTO noisyless.measurements
```

**Payload attendu :**
```json
{
  "villa_id": "villa_001",
  "total_count": 5,
  "heatmap": {"grid": [[0,1,2],[1,3,1],[0,1,0]]},
  "battery_mv": 3300
}
```

**Problèmes identifiés :**

| # | Problème | Impact |
|---|----------|--------|
| I1 | MQTT plain (pas TLS) | Sécurité |
| I2 | Pas d'auth MQTT (anonyme) | Sécurité |
| I3 | Pas de validation schéma `(model, schema)` | Extensibilité |
| I4 | `device_id` depuis topic, pas payload | Incohérence potentielle |

---

### 2.3 heatmap-agg

**Code :** `_infra/services/heatmap-agg/main.py`

**Fonctionnement :**
```python
Toutes les 60s:
  SELECT mesures des 5 dernières minutes
    ↓
  GROUP BY villa_id, zone
    ↓
  Merge grids (V1: dernier grid)
    ↓
  INSERT INTO heatmap_agg_5min (ON CONFLICT UPDATE)
```

**Problèmes identifiés :**

| # | Problème | Impact |
|---|----------|--------|
| A1 | Python polling 60s | Performance |
| A2 | `merge_grids()` = dernier grid (pas moyenne) | Précision heatmap |
| A3 | `jsonb` pour grids (pas `int[]`) | Performance SQL |
| A4 | Pas de continuous aggregates TimescaleDB | Dette technique |

**Recommandation (Migration Plan) :**
```sql
CREATE MATERIALIZED VIEW heatmap_agg_5min
WITH (timescaledb.continuous) AS
SELECT 
  time_bucket('5 minutes', time) AS window_start,
  villa_id,
  zone,
  SUM((payload->>'total_count')::int) AS total_count,
  MAX((payload->>'max_count')::int) AS max_count,
  AVG((payload->'heatmap'->'grid')::int[]) AS grid_avg  -- À adapter
FROM measurements
WHERE model = 'nl-map-in'
GROUP BY window_start, villa_id, zone;
```

---

### 2.4 alert-engine

**Code :** `_infra/services/alert-engine/main.py`

**Fonctionnement :**
```python
Toutes les 30s:
  SELECT seuils FROM alert_thresholds
    ↓
  CHECK dernier agrégat heatmap_agg_5min
    ↓
  IF max_count > threshold → Telegram alert
    ↓
  Anti-spam: 1 alerte / fenêtre 5min
```

**État :** ✅ Fonctionnel, mais polling trop rapide (30s vs 5min agrégat)

**Recommandation :** Poller toutes les 5min ou utiliser `LISTEN/NOTIFY` PostgreSQL.

---

### 2.5 FastAPI Backend

**Code :** `_infra/services/api/main.py`

**Endpoints :**
| Method | Endpoint | Auth |
|--------|----------|------|
| GET | `/health` | ❌ |
| GET | `/api/devices` | ✅ JWT |
| POST | `/api/devices` | ✅ JWT |
| GET | `/api/devices/{id}/live` | ✅ JWT |
| GET | `/api/devices/{id}/timeline` | ✅ JWT |
| GET | `/api/villas/{id}/heatmap` | ✅ JWT |

**Auth :** JWT (HS256), secret devinable (`noisyless_jwt_super_secret_key_2026`)

**Problèmes identifiés :**

| # | Problème | Impact |
|---|----------|--------|
| B1 | `JWT_SECRET` devinable | Sécurité 🔴 |
| B2 | `DB_PASSWORD` devinable | Sécurité 🔴 |
| B3 | Pas de refresh tokens | UX |
| B4 | Pas de claim/provisioning endpoint | Fonctionnel |
| B5 | Pas de webhook abonnement | Intégration tierce |

---

## 3. Infrastructure — État actuel

### 3.1 Serveur VPS

| Élément | Valeur |
|---------|--------|
| IP | 91.99.26.43 |
| Fournisseur | OVH |
| DNS | Cloudflare (propagation en cours) |
| SSH | `noisyless@91.99.26.43` (id_ed25519) |

### 3.2 Docker Compose

**Fichier :** `_infra/docker-compose.yml`

**Services déployés :**
```yaml
mosquitto:1883,8883    — MQTT broker (TLS non activé)
timescaledb:5432       — DB (localhost only)
mqtt-ingest            — Ingest service
heatmap-agg            — Aggregation
alert-engine           — Alertes Telegram
api:8000               — FastAPI backend
nginx:80,443           — Reverse proxy + SSL
```

**Réseau :** `noisyless-net` (bridge)

### 3.3 SSL/TLS

**État :** ✅ Let's Encrypt généré via Cloudflare DNS-01

**Certificats :**
- Domaine : `platform.noisyless.com`, `api.platform.noisyless.com`
- Validité : 90 jours
- Stockage : `/opt/noisyless/nginx/ssl/`
- Renouvellement : Cron mensuel (`/opt/noisyless/renew-ssl.sh`)

**Problème :** Redondant avec tunnel Cloudflare (voir Migration Plan)

### 3.4 MQTT — Sécurité

| Couche | État | Cible |
|--------|------|-------|
| Transport | Plain 1883 | TLS 8883 |
| Auth | Aucune | Username=DUID + password |
| ACL | Aucune | `noisyless/<duid>/#` |

**Impact :** 🔴 Critique avant production

---

## 4. AOS-Core (192.168.0.176) — État

**Répertoire :** `/home/mathieu/noisyless/` (quasiment vide)

```
/home/mathieu/noisyless/
└── drafts-a-valider/
    └── 5-hotels.md   — Prospection hôtels
```

**Constats :**
- Aucun firmware sur 192.168.0.176
- Aucun service backend sur 192.168.0.176
- Toute l'infra est sur VPS distant (91.99.26.43)
- AOS-Core = environnement de dev local, pas de prod NOISYLESS

**Recommandation :** Centraliser dev ET prod sur VPS, ou documenter clairement la séparation.

---

## 5. Synthèse — Ce qui est fait vs À faire

### ✅ Fait (prêt pour prod)

| Composant | État | Notes |
|-----------|------|-------|
| Firmware TOF (ranging + cluster) | ✅ | ESP-IDF, TMF8829, clustering 4-way |
| Mesh ESP-NOW | ✅ | Élection master/slave |
| mqtt-ingest | ✅ | Subscribe → TimescaleDB |
| heatmap-agg | ✅ | Agrégation 5min (Python) |
| alert-engine | ✅ | Telegram alerts |
| FastAPI | ✅ | JWT auth, endpoints CRUD |
| Nginx + SSL | ✅ | Let's Encrypt Cloudflare |
| Docker Compose | ✅ | Tous services conteneurisés |

---

### 🔴 À faire (Bloc 1 — vendable)

| Composant | Travail | Effort | Risque |
|-----------|---------|--------|--------|
| **Firmware MQTT** | Implémenter publish dans `master_task()` | 4h | ⚪ |
| **Provisioning WiFi** | SoftAP + claim HMAC | 8h | 🟠 |
| **MQTT TLS** | Cert serveur + pinning firmware | 6h | 🟠 |
| **MQTT Auth** | `password_file` + ACL par DUID | 4h | 🟠 |
| **Secrets générés** | `JWT_SECRET`, `DB_PASSWORD` aléatoires | 1h | ⚪ |
| **NVS chiffré** | `nvs_flash_encrypt()` | 4h | 🟠 |
| **Continuous aggregates** | Remplacer `heatmap-agg` Python | 6h | 🟠 |
| **Bootstrap/restore** | Scripts reproductibles | 4h | ⚪ |
| **Backups chiffrés** | `pg_dump` + age + git | 3h | ⚪ |

**Total Bloc 1 : ~40h** (2 semaines)

---

### 🟡 À faire (Bloc 2 — scale)

| Composant | Travail |
|-----------|---------|
| OTA signée Ed25519 | Clé privée hors CI, A/B partition, rollback |
| Secure Boot v2 | Activation irréversible (après OTA validée) |
| Driver radar 60GHz | SDK TI + intégration mesh |
| Agrégation heatmap `int[]` | Fusion cellule par cellule (pas jsonb) |
| Grafana + métriques | Monitoring parc |

---

### ⚪ À faire (Bloc 3 — indus)

| Composant | Travail |
|-----------|---------|
| EMQX cluster | Si >100 modules concurrents |
| Multi-tenant billing | Stripe + quota par module |
| `nl-map-out` IP67 | Boîtier extérieur, PoE |
| 2FA admin | TOTP |

---

## 6. Roadmap recommandée

### Semaine 1 — Sécurité de base
- [ ] Générer secrets aléatoires (`JWT_SECRET`, `DB_PASSWORD`)
- [ ] MQTT auth + ACL (`password_file`, `acl_file`)
- [ ] MQTT TLS 8883 (cert serveur)
- [ ] Firmware : publish MQTT dans `master_task()`

### Semaine 2 — Claim + Provisioning
- [ ] SoftAP provisioning (WiFi + claim)
- [ ] Endpoint API `/v1/devices/claim`
- [ ] Mail Brevo validation
- [ ] NVS chiffré firmware

### Semaine 3 — Simplification backend
- [ ] Continuous aggregates TimescaleDB
- [ ] `alert-engine` → polling 5min
- [ ] Scripts bootstrap/restore
- [ ] Backups chiffrés age

### Semaine 4 — Test pilote
- [ ] Déploiement villa pilote
- [ ] Tests end-to-end
- [ ] Itération retours client

---

## 7. Décisions requises avant attaque

| Sujet | Décision attendue |
|-------|-------------------|
| MQTT TLS maintenant ou après ? | **Maintenant** (Bloc 1) |
| EMQX ou Mosquitto ? | **Mosquitto** (Bloc 1), EMQX (Bloc 3) |
| Tunnel Cloudflare ou SSL direct ? | **Tunnel** (garder), SSL local à retirer |
| Modèle éco : par module ou par compte ? | **Par module** (gating DUID) |
| AOS-Core (192.168.0.176) ou VPS (91.99.26.43) ? | **VPS** pour prod, AOS-Core dev local |

---

*Document créé : 24 Mai 2026*  
*Prochaine étape : Exécution Bloc 1, chantier par chantier*
