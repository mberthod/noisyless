# NOISYLESS v1 — État des lieux (29 mai 2026)

> Snapshot complet du système en production. Capture de ce qui tourne réellement
> sur le VPS Hetzner (91.99.26.43), synchronisé avec le repo GitHub.

---

## Architecture production

```
noisyless.com (marketing)
  └── nginx → /var/www/static/index.html + produits/, cas-usage/, etc.

platform.noisyless.com (dashboard)
  └── nginx → /var/www/static/platform.html
                ├── /admin.html (admin panel)
                ├── /dashboard.html (user dashboard)
                ├── /api/* → api container:8000
                └── /admin/* → api container:8000/admin/
```

## Conteneurs Docker (8)

| Conteneur | IP | Status | Rôle |
|-----------|----|--------|------|
| noisyless-api | 172.18.0.2 | Up 18h | FastAPI backend (auth, devices, shop, admin) |
| noisyless-timescaledb | 172.18.0.3 | Up 4d | TimescaleDB PG16 |
| noisyless-alert-engine | 172.18.0.4 | Up 4d | ✅ Alertes seuils |
| noisyless-heatmap-agg | 172.18.0.5 | Up 4d | Agrégation HeatMap 5 min |
| noisyless-nginx | 172.18.0.6 | Up 16h | Reverse proxy (2 domaines) |
| noisyless-mosquitto | 172.18.0.7 | Up 4d | Broker MQTT (1883, 8883) |
| noisyless-mqtt-ingest | 172.18.0.8 | Up 4d | MQTT → DB (HeatMap data) |
| noisyless-env-ingest | 172.18.0.9 | Up 45h | ESP32 → DB (env data) |

## DNS / Réseau

- **Hetzner VPS** : Ubuntu 7.0.0, 38GB disk (8G used), aarch64
- **iptables / firewall** : 80, 443, 1883, 8883 ouverts
- **SSL** : LetsEncrypt via certbot (auto-renouvellement)

---

## Pages web — Site public (noisyless.com)

### Accueil
- `/` → index.html ✅ 200

### Pages légales
- `/faq.html` ✅
- `/cgv.html` ✅
- `/confidentialite.html` ✅
- `/mentions-legales.html` ✅
- `/annule.html` ✅
- `/merci.html` ✅

### Produits
- `/produits/heatmap.html` ✅
- `/produits/heatmap-indoor.html` ✅
- `/produits/heatmap-outdoor.html` ✅
- `/produits/heatmap-property-pack.html` ✅
- `/produits/noisyless-simple.html` ✅

### Cas d'usage
- `/cas-usage/conciergerie.html` ✅
- `/cas-usage/hospitality-resort.html` ✅
- `/cas-usage/immobilier-institutionnel.html` ✅
- `/cas-usage/villa-premium.html` ✅
- ❌ **URLs attendues** : copropriete, bureaux, logement-social, residence-senior → 404

### Comparatif
- `/comparatif/heatmap-vs-camera.html` ✅
- `/comparatif/heatmap-vs-detecteur.html` ✅
- `/comparatif/heatmap-vs-minut.html` ✅

### Technologie
- `/technologie/intelligence-spatiale.html` ✅
- `/technologie/privacy-by-design.html` ✅
- `/technologie/reseau-maille.html` ✅

### CTA
- `/boutique.html` ✅
- `/pilote.html` ✅
- `/pilote-heatmap.html` ✅
- `/precommande.html` ✅
- `/conciergerie.html` ✅
- `/dashboard.html` ✅

### Blog (10 articles)
- `/blog/index.html` ✅
- `/blog/channel-managers-monitoring.html`
- `/blog/degat-des-eaux-vanne-coupure-automatique.html`
- `/blog/detecter-fete-airbnb-sans-camera.html`
- `/blog/detecteur-bruit-airbnb-legal.html`
- `/blog/minut-vs-noisyless-comparatif.html`
- `/blog/nuisances-sonores-copropriete.html`
- `/blog/rgpd-capteurs-location-conformite.html`
- `/blog/suroccupation-location-courte-duree.html`
- `/blog/vapotage-cigarette-location-detection.html`

### SEO ❌
- `/robots.txt` → **404** (non déployé)
- `/sitemap.xml` → **404** (non déployé)
- Pourtant listés dans la config nginx du skill → à déployer

---

## Pages web — Plateforme (platform.noisyless.com)

- `/` → platform.html ✅ 200
- `/admin.html` ✅ 200
- `/dashboard.html` ✅

---

## Base de données

### Tables (schéma `noisyless.`)

| Table | Rôle |
|-------|------|
| users | 20 comptes |
| devices | 8 devices (0 online) |
| user_devices | Liaison users ↔ devices |
| measurements | Données environnementales (hypertable) |
| heatmap_agg_5min | Agrégats HeatMap 5 min |
| firmware_versions | 5 versions enregistrées |
| ota_deployments | Déploiements OTA |
| alert_thresholds | Seuils d'alerte |
| device_health | Santé des devices |
| auth_tokens | Tokens d'authentification |
| orders | Commandes boutique |
| order_items | Lignes de commande |

### Firmware versions en DB

| Version | SHA256 |
|---------|--------|
| 0.0.1 | eca3b180ec39b86c5... |
| 0.0.2 | b366af87ffa4e2e2d... |
| 1.0.1 | afde99ed3d12cb8e2... |
| 1.0.2 | 1280dfb513ccea72c... |
| 1.1.0-wifimanager | 703182fd39b96e30f... |

### Firmware binaires sur VPS

| Fichier | Taille | Statut |
|---------|--------|--------|
| noisyless-v0.0.1.bin | 1,083,440 | Déployé |
| noisyless-v0.0.2.bin | 1,083,472 | Déployé |
| noisyless-v0.0.3.bin | 1,083,984 | Déployé |
| noisyless-v1.0.1.bin | 879,504 | Déployé |
| noisyless-v1.0.2.bin | 879,376 | Déployé |
| noisyless-v1.1.0-wifimanager.bin | 1,083,056 | WiFi Manager intégré |
| firmware.bin | 1,083,984 | Copie sympa de v0.0.3 |

---

## Services backend (Python/FastAPI)

### API (12 fichiers)
- `main.py` — App FastAPI, routes, DB pool
- `auth.py` — Inscription, login, vérification email, reset password (bcrypt, JWT)
- `admin.py` — Panel admin : devices, overview, firmware, users (⚠️ seulement sur VPS)
- `devices.py` — CRUD devices, registration, claim
- `customer.py` — Espace client
- `emails.py` — Envoi emails (SMTP OVH)
- `pilot.py` — Programme pilote
- `shop.py` — Boutique Stripe
- `shop_webhook.py` — Webhooks Stripe
- `Dockerfile`, `requirements.txt`

### Micro-services (4)
- `mqtt-ingest/` — HeatMap data (MQTT → TimescaleDB)
- `env-ingest/` — ESP32 capteurs environnement (MQTT → TimescaleDB)
- `heatmap-agg/` — Agrégation 5 min des heatmaps
- `alert-engine/` — Alertes seuils (Telegram)

---

## Bugs v1 connus

### 🔴 alert-engine : ConnectionRefusedError
```
ConnectionRefusedError: [Errno 111] Connect call failed ('172.18.0.3', 5432)
```
- L'alert-engine essaie de se connecter à la DB sur 172.18.0.3:5432 mais la connexion est refusée
- TimescaleDB pourtant up (healthy) sur la même IP
- **À investiguer** : problème de réseau Docker, dépendance de démarrage, ou changement d'IP

### 🔴 API : send_verification_email() parameter mismatch
```
TypeError: send_verification_email() missing 1 required positional argument: 'background_tasks'
```
- Dans `auth.py`, la fonction attend un paramètre `background_tasks: BackgroundTasks`
- Mais elle est appelée sans ce paramètre quelque part
- **Fix** : aligner l'appel dans main.py ou les routes

### 🟡 heatmap-agg : No data for window
- L'agrégateur tourne toutes les 5 min mais ne trouve aucune donnée
- Pourtant mqtt-ingest reçoit bien des messages HeatMap → les données arrivent
- **À investiguer** : peut-être un décalage de timestamp, ou les heatmap_agg_5min ne sont pas peuplées

### 🟡 8 devices offline
- Tous les devices enregistrés en DB ont status = 'offline'
- Dernière activité : NL-B43A45B7A9B0 (env-ingest reçoit encore des datas)
- **Hypothèse** : heartbeat non mis à jour, ou les ESP32 déconnectés

### 🟡 4 pages cas-usage 404
- Les URLs attendues (copropriete, bureaux, logement-social, residence-senior) ne correspondent pas aux fichiers déployés (conciergerie, hospitality-resort, immobilier-institutionnel, villa-premium)
- **Deux choix** : renommer les fichiers ou corriger les URLs dans le site

### 🟡 robots.txt + sitemap.xml 404
- Pas déployés sur le VPS alors qu'ils étaient listés dans la config nginx prévue
- **Fix** : générer et déployer

---

## GitHub vs VPS — Écarts résolus

Ce commit synchronise tous les fichiers manquants entre le VPS et le repo :

| Fichier | Source | Destination |
|---------|--------|-------------|
| `admin.py` | VPS only | _infra + backend |
| `env-ingest/` (3 fichiers) | VPS only | _infra |
| `platform.html` | VPS only | _infra + web/site |
| `admin.html` | VPS only | _infra + web/site |
| `dashboard.html` | VPS only | _infra + web/site |
| `pilote-heatmap.html` | VPS only | _infra + web/site |
| `precommande.html` | VPS only | _infra + web/site |
| `conciergerie.html` | VPS only | _infra + web/site |
| `produits/heatmap-*.html` (3) | VPS only | _infra + web/site |
| `cas-usage/*.html` (4) | VPS only | _infra + web/site |
| `comparatif/*.html` (3) | VPS only | _infra + web/site |
| `technologie/*.html` (3) | VPS only | _infra + web/site |
| `nginx.conf` | Updated VPS | _infra |
| `docker-compose.yml` | Updated VPS | _infra |
| `mosquitto.conf` | VPS | _infra |
| `firmware/stable.json` | VPS | _infra |
| `firmware/latest.json` | VPS only | _infra |
| `auth.py`, `devices.py`, `main.py` | VPS version | _infra (remplace) |

**Notes :**
- Les fichiers `index.html`, `faq.html`, etc. déjà dans le repo n'ont pas été écrasés (ils sont présents dans `_live/static` pour info)
- Les binaires firmware ne sont pas commités dans `_live/` (trop lourds), mais copiés dans `01_PROJETS/202604_089_Capteur_Environnemental_V1/05_FIRMWARE/` et `ota/`
- Le dossier `_live/` est un snapshot de référence — pas un dossier d'infrastructure actif
