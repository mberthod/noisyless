# NOISYLESS - Architecture Technique

## Vue d'ensemble

NOISYLESS est une plateforme IoT de surveillance d'occupation par capteurs ToF (Time of Flight) et Radar. Le système collecte les données de présence, génère des heatmaps en temps réel, et alerte en cas de surpopulation.

**Projet :** #091 - HeatMap Présence  
**Serveur :** 91.99.26.43 (OVH)  
**Domaine :** platform.noisyless.com

---

## Architecture Logicielle

```
┌─────────────────────────────────────────────────────────────────────┐
│                         NOISYLESS PLATFORM                          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌──────────────┐     ┌──────────────┐     ┌──────────────┐        │
│  │   Devices    │────▶│    Mosquitto │────▶│  MQTT Ingest │        │
│  │  (ToF/Radar) │ MQTT│   Broker     │ MQTT│   Service    │        │
│  │  ESP32-S3    │     │  :1883/:8883 │     │              │        │
│  └──────────────┘     └──────────────┘     └──────┬───────┘        │
│                                                    │                 │
│                                                    ▼                 │
│                                             ┌──────────────┐        │
│                                             │  TimescaleDB │        │
│                                             │  PostgreSQL  │        │
│                                             │    :5432     │        │
│                                             └──────┬───────┘        │
│                                                    │                 │
│           ┌─────────────────────────────────────────┼─────────┐     │
│           │                 │                       │         │     │
│           ▼                 ▼                       ▼         ▼     │
│    ┌────────────┐   ┌────────────┐          ┌──────────┐ ┌───────┐ │
│    │  Heatmap   │   │   Alert    │          │ FastAPI  │ │ Nginx │ │
│    │ Aggregator │   │   Engine   │          │ Backend  │ │ Proxy │ │
│    │            │   │            │          │          │ │       │ │
│    └────────────┘   └─────┬──────┘          └────┬─────┘ └───┬───┘ │
│                           │                      │           │     │
│                           ▼                      │           │     │
│                    ┌─────────────┐                │           │     │
│                    │  Telegram   │                │           │     │
│                    │   Alerts    │                │           │     │
│                    └─────────────┘                │           │     │
│                                                   │           │     │
│                                             ┌─────┴───────────┴───┐ │
│                                             │   HTTPS :443      │ │
│                                             │   HTTP  :80       │ │
│                                             └─────────┬─────────┘ │
│                                                       │           │
└───────────────────────────────────────────────────────┼───────────┘
                                                        │
                                              ┌─────────▼─────────┐
                                              │   Web Client      │
                                              │   Mobile App      │
                                              │   Third-party API │
                                              └───────────────────┘
```

---

## Composants

### 1. MQTT Broker (Eclipse Mosquitto)

**Image :** `eclipse-mosquitto:2`  
**Ports :** 1883 (MQTT), 8883 (MQTT TLS)  
**Rôle :** Message broker pour les devices IoT

**Topics :**
- `noisyless/+/heatmap` - Données de présence des capteurs
- `noisyless/+/config` - Configuration OTA (futur)
- `noisyless/+/status` - Télémétrie devices (futur)

**Config :** `/opt/noisyless/mosquitto/config/mosquitto.conf`

---

### 2. TimescaleDB (PostgreSQL + Time Series)

**Image :** `timescale/timescaledb:latest-pg16`  
**Port :** 5432 (localhost only)  
**Rôle :** Base de données time-series pour les mesures

**Schéma :** `noisyless`

**Tables principales :**

| Table | Description | Rétention |
|-------|-------------|-----------|
| `users` | Utilisateurs plateforme | Permanent |
| `devices` | Enregistrement des capteurs | Permanent |
| `user_devices` | Liaison users/devices | Permanent |
| `measurements` | Mesures brutes (hypertable) | 90 jours |
| `heatmap_agg_5min` | Heatmaps agrégées 5min | 30 jours |
| `alert_thresholds` | Seuils d'alerte par villa | Permanent |

**Hypertable :** `measurements` partitionné par `time`

---

### 3. MQTT Ingest Service

**Langage :** Python 3.11  
**Source :** `services/mqtt-ingest/main.py`  
**Rôle :** Subscribe MQTT → Insert DB

**Fonctionnement :**
1. Subscribe au topic `noisyless/+/heatmap`
2. Parse le payload JSON
3. Extrait `device_id` du topic
4. Insert dans `noisyless.measurements`

**Payload attendu :**
```json
{
  "villa_id": "villa_001",
  "total_count": 5,
  "heatmap": {"grid": [[0,1,2],[1,3,1],[0,1,0]]},
  "battery_mv": 3300
}
```

---

### 4. Heatmap Aggregator

**Langage :** Python 3.11  
**Source :** `services/heatmap-agg/main.py`  
**Rôle :** Agrège les mesures en heatmaps 5min

**Fonctionnement :**
1. Toutes les 60 secondes
2. Lit les mesures des 5 dernières minutes
3. Regroupe par `villa_id` et `zone`
4. Fusionne les grids (moyenne)
5. Insert dans `heatmap_agg_5min`

**Fenêtre :** 300 secondes (configurable via `AGG_WINDOW_SECONDS`)

---

### 5. Alert Engine

**Langage :** Python 3.11  
**Source :** `services/alert-engine/main.py`  
**Rôle :** Surveillance seuils → Notifications

**Fonctionnement :**
1. Toutes les 30 secondes
2. Lit `alert_thresholds` par villa
3. Compare avec dernière aggregation
4. Si `max_count > threshold` → Alert Telegram

**Notifications :**
- Telegram Bot (configurable)
- Anti-spam : 1 alerte max par fenêtre 5min

---

### 6. FastAPI Backend

**Langage :** Python 3.11 + FastAPI  
**Source :** `services/api/main.py`  
**Rôle :** API REST pour clients web/mobile

**Authentification :** JWT (HS256)

**Endpoints :**

| Method | Endpoint | Description | Auth |
|--------|----------|-------------|------|
| GET | `/health` | Health check | ❌ |
| GET | `/api/devices` | Liste des devices | ✅ |
| POST | `/api/devices` | Nouveau device | ✅ |
| GET | `/api/devices/{id}/live` | Données temps réel | ✅ |
| GET | `/api/devices/{id}/timeline` | Historique (24h) | ✅ |
| GET | `/api/villas/{id}/heatmap` | Heatmap agrégée | ✅ |

**Sécurité :**
- JWT expiry : 30 minutes
- Password hashing : bcrypt
- CORS : à configurer

---

### 7. Nginx Reverse Proxy

**Image :** `nginx:alpine`  
**Ports :** 80 (HTTP), 443 (HTTPS)  
**Rôle :** Reverse proxy + SSL termination

**Configuration :**
- HTTP → Redirect HTTPS
- HTTPS → Proxy vers API (:8000)
- Static files : `/var/www/static`
- SSL : Let's Encrypt (Cloudflare DNS)

**Certificats :**
- Domaine : `platform.noisyless.com`, `api.platform.noisyless.com`
- Validité : 90 jours (renouvellement auto mensuel)
- Stockage : `/opt/noisyless/nginx/ssl`

---

## Infrastructure

### Serveur

| Spécification | Valeur |
|--------------|--------|
| Fournisseur | OVH |
| IP | 91.99.26.43 |
| OS | Linux (à préciser) |
| Docker | v27+ |
| DNS | Cloudflare |

### DNS (Cloudflare)

| Type | Name | Content | Proxy |
|------|------|---------|-------|
| A | platform | 91.99.26.43 | DNS only |
| A | api.platform | 91.99.26.43 | DNS only |

### SSL/TLS

- **Autorité :** Let's Encrypt
- **Validation :** DNS-01 (Cloudflare API)
- **Renouvellement :** Cron mensuel (1er à 03:00)
- **Script :** `/opt/noisyless/renew-ssl.sh`

---

## Réseau Docker

**Network :** `noisyless-net` (bridge)

**Communication interne :**

| Service | Port | Accessible depuis |
|---------|------|-------------------|
| mosquitto | 1883 | mqtt-ingest |
| timescaledb | 5432 | Tous services |
| api | 8000 | nginx uniquement |
| nginx | 80/443 | Internet |

---

## Variables d'environnement

### Fichier `.env` (à créer sur le serveur)

```bash
# Database
DB_PASSWORD=noisyless_secret_password

# JWT
JWT_SECRET=noisyless_jwt_super_secret_key_2026
JWT_EXPIRY_MINUTES=30

# Telegram Alerts
TELEGRAM_BOT_TOKEN=<token>
TELEGRAM_CHAT_ID=<chat_id>
```

### Emplacement
`/opt/noisyless/.env`

---

## Déploiement

### Prérequis
- Docker & Docker Compose
- SSH access : `~/.ssh/id_ed25519`
- Cloudflare API token

### Commandes

```bash
# Connexion au serveur
ssh -i ~/.ssh/id_ed25519 noisyless@91.99.26.43

# Démarrage
cd /opt/noisyless
docker compose up -d

# Logs
docker compose logs -f

# Redémarrage
docker compose restart

# Arrêt
docker compose down
```

---

## Surveillance

### Logs

```bash
# Tous services
docker compose logs -f

# Service spécifique
docker compose logs -f mqtt-ingest

# Nginx
docker exec noisyless-nginx tail -f /var/log/nginx/access.log
```

### Santé

```bash
# API health
curl -k https://platform.noisyless.com/health

# DB connection
docker exec noisyless-timescaledb pg_isready -U noisyless

# MQTT broker
docker exec noisyless-mosquitto mosquitto_sub -t '#' -v
```

### Métriques (à implémenter)

- CPU/RAM par container
- DB size & growth
- MQTT messages/sec
- API response time
- Error rate

---

## Sécurité

### En place

- ✅ HTTPS (Let's Encrypt)
- ✅ JWT authentication API
- ✅ DB password protégée
- ✅ TimescaleDB localhost only
- ✅ Container isolation (network)

### À améliorer

- [ ] MQTT authentication (username/password)
- [ ] MQTT TLS (port 8883)
- [ ] Rate limiting API
- [ ] Firewall rules (ufw)
- [ ] Fail2ban
- [ ] Backup automatique DB
- [ ] Monitoring (Prometheus/Grafana)

---

## Sauvegarde

### Base de données

```bash
# Backup manuel
docker exec noisyless-timescaledb pg_dump -U noisyless noisyless > backup_$(date +%Y%m%d).sql

# Restore
cat backup_YYYYMMDD.sql | docker exec -i noisyless-timescaledb psql -U noisyless noisyless
```

### À automatiser

- [ ] Backup quotidien (cron)
- [ ] Rotation 7 jours
- [ ] Stockage externe (S3, etc.)

---

## Évolution

### Roadmap

**V1.1 - Sécurité MQTT**
- Authentification broker
- Chiffrement TLS
- Certificats devices

**V1.2 - Dashboard**
- Interface web temps réel
- Visualisation heatmaps
- Historique graphs

**V1.3 - Firmware OTA**
- Mise à jour devices à distance
- Topic `noisyless/+/ota`

**V2.0 - Multi-tenant**
- Isolation par client
- Quotas & billing
- White-label

---

## Dépannage

### Problèmes courants

**API ne répond pas :**
```bash
docker compose logs api
docker compose restart api
```

**DB connection failed :**
```bash
docker compose ps timescaledb
docker compose logs timescaledb
```

**Certificat expiré :**
```bash
/opt/noisyless/renew-ssl.sh
```

**MQTT messages non reçus :**
```bash
docker exec noisyless-mosquitto mosquitto_sub -t 'noisyless/+/heatmap' -v
```

---

## Contact

**Développeur :** NOISYLESS Team  
**Email :** contact@noisyless.com  
**Documentation :** `/opt/noisyless/README.md`

---

*Dernière mise à jour : 24 Mai 2026*
