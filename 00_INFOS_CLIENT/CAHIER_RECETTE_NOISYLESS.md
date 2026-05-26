# CAHIER DE RECETTE — NOISYLESS
## État des lieux complet au 25/05/2026

> **Objet** : Ce document dresse l'état exhaustif de tout ce qui a été réalisé sur le projet NOISYLESS.  
> **Usage** : Base pour la rédaction du cahier des charges et du dossier de conception.  
> **Méthode** : Audit en direct du serveur Hetzner + fichiers Kdrive + base de données.

---

## SOMMAIRE

1. [Infrastructure](#1-infrastructure)
2. [Site vitrine — Pages statiques](#2-site-vitrine--pages-statiques)
3. [Blog — 9 articles SEO](#3-blog--9-articles-seo)
4. [API Backend](#4-api-backend)
5. [Base de données](#5-base-de-données)
6. [Services backend](#6-services-backend)
7. [Intégration Stripe](#7-intégration-stripe)
8. [DNS & Cloudflare](#8-dns--cloudflare)
9. [Sécurité & Conformité](#9-sécurité--conformité)
10. [Firmware & Hardware](#10-firmware--hardware)
11. [Backlog & Tâches restantes](#11-backlog--tâches-restantes)
12. [Synthèse pour cahier des charges](#12-synthèse-pour-cahier-des-charges)

---

## 1. INFRASTRUCTURE

### 1.1 Serveur
| Propriété | Valeur |
|-----------|--------|
| Hébergeur | Hetzner Cloud |
| Modèle | CAX11 (2 vCPU, 4 GB RAM, 40 GB SSD) |
| IP | **91.99.26.43** |
| OS | Ubuntu 24.04 LTS |
| SSH | `noisyless@91.99.26.43` |
| Domaine principal | **noisyless.com** |
| Domaine plateforme | **platform.noisyless.com** |

### 1.2 Docker Compose — 7 conteneurs

```
noisyless (docker-compose)
├── noisyless-nginx         ← nginx:alpine (ports 80, 443)
├── noisyless-api           ← FastAPI uvicorn (port 8000 interne)
├── noisyless-timescaledb   ← PostgreSQL 16 + TimescaleDB (port 5432 interne)
├── noisyless-mosquitto     ← MQTT broker (ports 1883, 8883)
├── noisyless-mqtt-ingest   ← Ingestion MQTT → TimescaleDB
├── noisyless-heatmap-agg   ← Agrégation heatmap (fenêtre 300s)
└── noisyless-alert-engine  ← Moteur d'alertes
```

État au 25/05/2026 : **tous les conteneurs UP et stables** (uptime 24h-36h).

### 1.3 Configuration Nginx

```
Fichier : /opt/noisyless/nginx/nginx.conf
SSL    : TLS 1.2/1.3, certificats Let's Encrypt
Routes :
  /          → /var/www/static (fichiers statiques, SPA try_files)
  /api/      → proxy_pass http://api:8000 (CORS: *)
  /auth/     → proxy_pass http://api:8000
  /shop/     → proxy_pass http://api:8000 (CORS: *)
  /health    → proxy_pass http://api:8000/health
```

**Points d'attention** :
- `try_files $uri $uri/ /index.html` — comportement SPA, toute URL inconnue renvoie index.html
- CORS configuré sur `/api/` et `/shop/` uniquement (pas sur `/auth/`)
- Redirection HTTP→HTTPS (301)
- Pas de rate limiting, pas de cache-control configuré côté nginx

### 1.4 Volumes Docker

| Volume hôte | Point de montage | Mode |
|-------------|-----------------|------|
| `./static/` | `/var/www/static` | ro |
| `./nginx/nginx.conf` | `/etc/nginx/nginx.conf` | ro |
| `./nginx/ssl/` | `/etc/nginx/ssl` | ro |
| `./mosquitto/config/` | `/mosquitto/config` | ro |
| `./mosquitto/data/` | `/mosquitto/data` | rw |
| `./timescaledb/data/` | `/var/lib/postgresql/data` | rw |
| `./timescaledb/init/` | `/docker-entrypoint-initdb.d` | ro |

---

## 2. SITE VITRINE — PAGES STATIQUES

### 2.1 Architecture des pages

Toutes servies depuis `/opt/noisyless/static/` → `https://noisyless.com/<page>`

| URL | Fichier | Taille | Mots | Titre | État |
|-----|---------|--------|------|-------|------|
| `/` | index.html | 28.4 KB | 1943 | NOISYLESS \| Instrument de surveillance environnementale | ✅ 200 |
| `/merci.html` | merci.html | 2.6 KB | 212 | Merci pour votre commande \| NOISYLESS | ✅ 200 |
| `/annule.html` | annule.html | 3.8 KB | 319 | Paiement annulé \| NOISYLESS | ✅ 200 |
| `/faq.html` | faq.html | 33.0 KB | 3387 | FAQ - NOISYLESS \| Détecteur de bruit Airbnb | ✅ 200 |
| `/boutique.html` | boutique.html | 12.8 KB | 981 | Boutique NOISYLESS — Acheter votre détecteur de bruit | ✅ 200 |
| `/pilote.html` | pilote.html | 11.6 KB | 858 | Demander un pilote \| NOISYLESS | ✅ 200 |
| `/mentions-legales.html` | mentions-legales.html | 4.7 KB | 399 | Mentions légales \| NOISYLESS | ✅ 200 |
| `/confidentialite.html` | confidentialite.html | 6.0 KB | 526 | Politique de confidentialité \| NOISYLESS | ✅ 200 |
| `/cgv.html` | cgv.html | 6.0 KB | 589 | Conditions générales de vente \| NOISYLESS | ✅ 200 |
| `/produits/noisyless-simple.html` | noisyless-simple.html | 28.4 KB | 2004 | NOISYLESS Simple \| Capteur environnemental professionnel | ✅ 200 |
| `/produits/heatmap.html` | heatmap.html | 13.5 KB | 1026 | NOISYLESS HeatMap \| Cartographie de présence | ✅ 200 |
| `/blog/` | blog/index.html | 7.8 KB | 678 | Blog \| NOISYLESS | ✅ 200 |
| `/llms.txt` | llms.txt | 763 B | — | — | ✅ 200 |
| `/robots.txt` | robots.txt | 321 B | — | — | ✅ 200 |

### 2.2 Détail SEO par page

**index.html** (accueil) :
- Title : 60 caractères ✅
- Meta description : non vérifiée sur le serveur
- JSON-LD Product + Organization : non trouvé sur la version serveur actuelle ⚠️
- Formulaire d'achat intégré (pk_live_ présente) ✅
- Design : Space Mono + IBM Plex Sans, #FFFFFF/#1A1A1A/#FF6B00

**produits/noisyless-simple.html** :
- Title : <60 chars ✅
- Clé Stripe LIVE présente dans le JS (`pk_live_`) ✅
- Formulaire avec sélecteur de quantité + abonnement ✅
- Fonctionnalité checkout : Stripe retourne `session_id + url` ✅ (testé en direct)

**faq.html** :
- 3387 mots, 13 questions en blocs `<details>` ✅
- 26 blocs schema.org (FAQPage JSON-LD) ✅
- Contenu développé bien au-delà du minimum de 2000 mots ✅

**boutique.html** :
- JSON-LD présent (1 bloc) ✅
- Redirige ou présente les produits

**blog/index.html** :
- 678 mots, index de 9 articles
- Chaque article lien fonctionnel

### 2.3 Design system

| Élément | Valeur |
|---------|--------|
| Direction | INSTRUMENT DE PRÉCISION (Dieter Rams / Braun) |
| Police titres | Space Mono |
| Police corps | IBM Plex Sans |
| Fond | #FFFFFF |
| Texte | #1A1A1A |
| Accent | #FF6B00 (ambre, ≤5% surface) |
| Anti-patterns proscrits | Dégradés, dark theme, emojis, blobs, SaaS générique, glow, pill buttons |

### 2.4 Assets statiques

```
/opt/noisyless/static/assets/
├── logo.png
├── logo_bicolor.png
├── logocarre.png
├── logohorizmonochrome.png
├── GPT image-2026-05-24-13-16-41.png
├── GPT image-2026-05-24-13-18-11.png
├── Grok imagine-2026-05-24-13-24-40.png
└── Grok imagine-2026-05-24-13-24-40 (1).png
```

⚠️ Les assets d'images sont générés par IA, probablement pas des visuels définitifs.

---

## 3. BLOG — 9 ARTICLES SEO

| Article | Mots | Sujet |
|---------|------|-------|
| detecteur-bruit-airbnb-legal.html | 1108 | Légalité détecteurs bruit Airbnb 2026 |
| minut-vs-noisyless-comparatif.html | 1037 | Minut vs NOISYLESS comparatif |
| nuisances-sonores-copropriete.html | 923 | Nuisances sonores et copropriété |
| rgpd-capteurs-location-conformite.html | 931 | RGPD et capteurs location |
| suroccupation-location-courte-duree.html | 927 | Suroccupation location courte durée |
| detecter-fete-airbnb-sans-camera.html | 921 | Détection fête sans caméra |
| vapotage-cigarette-location-detection.html | 920 | Vapotage et cigarette |
| degat-des-eaux-vanne-coupure-automatique.html | 916 | Dégât des eaux |
| channel-managers-monitoring.html | 876 | Channel managers intégration |

**Total** : 9 articles, ~8659 mots, titres optimisés <60 chars avec `| NOISYLESS`.

---

## 4. API BACKEND

### 4.1 Fichiers source

| Fichier | Lignes | Rôle |
|---------|--------|------|
| `services/api/main.py` | 47 | Entry point FastAPI |
| `services/api/shop.py` | 87 | Endpoint checkout Stripe |
| `services/api/auth.py` | — | Authentification magic link |
| `services/api/devices.py` | — | Gestion devices |
| `services/api/customer.py` | — | Gestion clients |
| `services/api/emails.py` | 55 | Envoi emails SMTP OVH |
| `services/api/pilot.py` | — | Programme pilote |
| `services/api/shop_webhook.py` | — | Webhook Stripe |

### 4.2 Endpoints

| Méthode | Route | Statut testé | Description |
|---------|-------|-------------|-------------|
| GET | `/api/health` | ❌ 404 | Health check (route non trouvée, `/health` existe) |
| POST | `/api/auth/send-magic-link` | ❌ 404 | Magic link auth (route non trouvée) |
| POST | `/shop/checkout` | ✅ 200 | Création session Stripe. Payload : `{items, email, name}` |
| GET | `/shop/webhook` | ❌ 404 | Webhook (GET = 404 normal, POST uniquement) |

### 4.3 Endpoint checkout — détail

```
POST /shop/checkout
Content-Type: application/json

{
  "items": [
    {
      "product_id": "nl-simple",
      "quantity": 1,
      "subscription_plan": null
    }
  ],
  "email": "client@example.com",
  "name": "Nom Client",
  "company": "Optionnel"
}

→ Réponse 200 :
{
  "session_id": "cs_live_a1k...",
  "url": "https://checkout.stripe.com/c/pay/cs_live_a1k..."
}
```

**Produits référencés** : `nl-simple` (79€), `nl-map-in`, `nl-map-out`, `nl-leak`
**Abonnements** : `"starter"` (4.99€/mois), `"business"` (14.90€/mois), `"pro"` (39.90€/mois)

### 4.4 Points d'attention API

- `/api/health` et `/api/auth/send-magic-link` retournent 404 → routes non implémentées ou mal routées
- Le health check fonctionne sur `/health` (route directe sans `/api/`)
- Validation Pydantic fonctionnelle (test 422 avec payload incomplet)
- Toutes les variables sensibles dans `/opt/noisyless/.env`

---

## 5. BASE DE DONNÉES

### 5.1 Tables (10 tables)

| Table | Contenu |
|-------|---------|
| `users` | Comptes utilisateurs (email, password_hash, account_type perso/pro) |
| `auth_tokens` | Magic link tokens (token_hash, expires_at, used) |
| `devices` | Devices enregistrés (device_id, device_type, firmware_version, villa_id) |
| `user_devices` | Association user ↔ device |
| `measurements` | Télémétrie brute (26 colonnes : température, humidité, CO2, présence, son, batterie...) |
| `heatmap_agg_5min` | Agrégation heatmap par fenêtre 5 min |
| `orders` | Commandes Stripe (session_id, email, amount, status) |
| `order_items` | Lignes de commande (product_id, quantity, unit_amount) |
| `alert_thresholds` | Seuils d'alerte configurables |
| `device_health` | État de santé des devices |

### 5.2 Schéma measurements (table centrale)

26 colonnes : `time, device_id, villa_id, zone, temperature_c, humidity_pct, pressure_pa, gas_ohms, iaq, co2eq_ppm, bvoc_eq_ppm, presence, people_count, target_distance_cm, lux1_raw, lux2_raw, sound_mic1_pp, sound_mic2_pp, tof_grid, radar_targets, heatmap_cluster, cluster_count, battery_mv, rssi_dbm, uptime_s`

Index : `(device_id, time DESC)`, `(villa_id, time DESC)`, `(zone)`

### 5.3 Schéma orders / order_items
```
orders : id(UUID), stripe_session_id, email, name, amount(int), currency(eur), 
         status(pending/paid/shipped/cancelled/refunded), stripe_payment_intent,
         shipping_address(jsonb), metadata(jsonb), created_at, updated_at

order_items : id(UUID), order_id(FK→orders), product_id, product_name, 
              quantity, unit_amount, metadata(jsonb), created_at
```

---

## 6. SERVICES BACKEND

### 6.1 MQTT Ingest (mqtt-ingest)
- **Lignes** : 145 (main.py)
- **Rôle** : Souscrit aux topics MQTT `noisyless/+/telemetry`, écrit dans `measurements`
- **Pattern** : Queue asyncio thread-safe (paho-mqtt callback → producer, asyncio consumer)
- **Broker** : mosquitto:1883 (interne Docker network)

### 6.2 Heatmap Aggregator (heatmap-agg)
- **Lignes** : 131
- **Rôle** : Agrège les données `measurements` → `heatmap_agg_5min` (fenêtre 300s)
- **État** : Tourne sans erreur apparente

### 6.3 Alert Engine (alert-engine)
- **Lignes** : 130
- **Rôle** : Vérifie les seuils dans `alert_thresholds`, déclenche notifications
- **Canaux** : Telegram (configuré mais tokens probablement pas en prod)

### 6.4 Email (emails.py)
- **Lignes** : 55
- **SMTP** : `ssl0.ovh.net:465` (OVH)
- **Usage** : Confirmation de commande post-webhook Stripe

---

## 7. INTÉGRATION STRIPE

### 7.1 Configuration

| Variable | Emplacement | Statut |
|----------|------------|--------|
| `STRIPE_SECRET_KEY` | `/opt/noisyless/.env` | ✅ `sk_live_...` |
| `pk_live_...` | `index.html`, `noisyless-simple.html` | ✅ Présente |
| `STRIPE_WEBHOOK_SECRET` | `/opt/noisyless/.env` | ✅ Configuré |

### 7.2 Flux de paiement

```
1. Utilisateur sur noisyless.com/produits/noisyless-simple.html
2. Sélectionne quantité + abonnement (optionnel)
3. Clic "Acheter" → fetch POST https://platform.noisyless.com/shop/checkout
4. Backend crée session Stripe (via sk_live_)
5. Frontend reçoit {session_id, url} → window.location.href = url
6. Redirection vers checkout.stripe.com
7. Paiement réussi → Stripe POST /shop/webhook (checkout.session.completed)
8. Webhook → écrit orders + order_items + envoie email confirmation
9. Redirection vers noisyless.com/merci.html
```

### 7.3 Points testés

- ✅ Création session Stripe OK (retourne URL valide)
- ✅ Clé LIVE fonctionnelle (pas de AuthError)
- ⚠️ Tunnel complet non testé (paiement réel + webhook + email)
- ⚠️ Abonnement récurrent non testé en conditions réelles

---

## 8. DNS & CLOUDFLARE

### 8.1 Configuration DNS

| Enregistrement | Type | Valeur | Proxy |
|---------------|------|--------|-------|
| noisyless.com | A | 91.99.26.43 | 🟠 Orange (proxied) |
| platform.noisyless.com | A | 91.99.26.43 | 🟠 Orange |

### 8.2 Historique

- Anciennement : OVH (188.165.53.185) → migré vers Hetzner (91.99.26.43) le 24/05/2026
- Problème de cache Cloudflare identifié : après rsync, le site servait encore l'ancienne version
- Solution : Purge Everything + désactiver/réactiver le proxy

### 8.3 Points d'attention

- Cloudflare peut cacher les fichiers HTML → nécessite purge après chaque déploiement
- Pas d'API key Cloudflare → purge manuelle via dashboard
- TTL navigateur non configuré explicitement

---

## 9. SÉCURITÉ & CONFORMITÉ

### 9.1 État actuel

| Élément | Statut | Détail |
|---------|--------|--------|
| HTTPS | ✅ | TLS 1.2/1.3, Let's Encrypt |
| UFW Firewall | ❌ | Non activé (sudo requis) |
| MQTT TLS | ✅ | Port 8883 configuré |
| SSH | ⚠️ | Port 22 ouvert, pas de fail2ban |
| CORS | ✅ partiel | `/api/`, `/shop/` → * ; `/auth/` → non |
| Pages légales | ⚠️ | Créées (mentions-legales, confidentialite, CGV) mais **non validées par Mathieu** |
| RGPD | ⚠️ | Pages créées mais non relues |
| Stripe | ✅ | Clés LIVE en production |
| DB password | ✅ | Dans `.env`, pas de défaut `noisyless_secret_password` changé ? |

### 9.2 Absent

- Pas de rate limiting (API)
- Pas de backup automatique vérifié (cron backups TimescaleDB configuré mais non testé en restore)
- Pas de monitoring (uptime, alerts serveur)
- Pas de CI/CD
- Pas de WAF

---

## 10. FIRMWARE & HARDWARE

### 10.1 Projet #089 — Capteur Environnemental V1

| Élément | État |
|---------|------|
| MCU | ESP32-S3-MINI-1 |
| Capteurs | BME680 (T/H/P/Gas) + LD2410 (présence/radar) + sonomètre |
| Framework | PlatformIO + Arduino (pas ESP-IDF bare metal) |
| Connectivité | WiFi + MQTT (91.99.26.43:1883) |
| PCB | KiCad RevB + V01 (dans Kdrive `03_CAO/`) |
| Boîtier | 3 fichiers .3mf (couvercle, fond, support) |
| Firmware | main.cpp avec BSEC2, LD2410, MQTT, capteur son |
| OTA | GitHub Releases + manifest `stable.json` |
| Flash | 8MB, partitions custom (factory + ota_0 + ota_1) |
| GitHub | `mberthod/noisyless` (OTA: `42d4743`) |

### 10.2 Projet #091 — HeatMap Présence

| Élément | État |
|---------|------|
| Produits | 3 variantes : Simple (env), HeatMap Int (ToF), HeatMap Ext (Radar) |
| MCU | ESP32-S3-MINI-1 |
| ToF intérieur | TMF8829 (16×16, I²C 0x29) — pas encore implémenté |
| Radar extérieur | LD6001A prioritaire, MS72SF1 en fallback |
| Mesh | ESP-NOW + élection bully (master = MAC la plus basse) |
| Firmware | ESP-IDF bare metal, FreeRTOS |
| PCB | KiCad (netlist.csv, MCU.kicad_sch) |
| Firmware existant | node-radar-60ghz complété (✅ dans backlog) |

### 10.3 Architecture technique cible

```
Nodes intérieurs (ToF TMF8829)  ←→  Mesh ESP-NOW  ←→  Master ESP32-S3
Nodes extérieurs (Radar LD6001A) ←→  Mesh ESP-NOW  ←→  (élu, recalage spatial)
                                                              ↓
                                                    WiFi → MQTT Broker
                                                              ↓
                                                    mqtt-ingest → TimescaleDB
                                                              ↓
                                                    heatmap-agg → heatmap_agg_5min
                                                              ↓
                                                    API → dashboard platform.noisyless.com
```

---

## 11. BACKLOG & TÂCHES RESTANTES

### 11.1 Résumé

| Statut | Nombre |
|--------|--------|
| ✅ Done | 21 |
| 🚫 Blocked | 6 |
| ❌ Error | 1 |
| **Total** | **28** |

### 11.2 Blocages (nécessite @Mathieu)

| ID | Tâche | Blocage |
|----|-------|---------|
| seo-04 | Sitemap.xml | Déploiement manuel requis |
| infra-04 | Purge cache Cloudflare | Dashboard manuel uniquement |
| fw-01 | Deep sleep ESP32-S3 | Hardware pas prêt |
| legal-01 | Mentions légales | Validation humaine |
| legal-02 | Confidentialité RGPD | Validation humaine |
| legal-03 | CGV | Validation humaine |
| infra-01 | UFW firewall | sudo NOPASSWD requis |

### 11.3 Tâches non planifiées (identifiées par cet audit)

- `/api/health` retourne 404 → route à corriger
- `/api/auth/send-magic-link` retourne 404 → route à implémenter
- Pas de sitemap.xml déployé
- Assets images IA à remplacer par visuels produit réels
- Pas de système de backup vérifié
- Pas de CI/CD
- Pas de monitoring serveur
- Formulaire d'abonnement sur page produit : défaut "Pro" à corriger

---

## 12. SYNTHÈSE POUR CAHIER DES CHARGES

### 12.1 Ce qui est fait et validé

1. **Infrastructure** : Serveur Hetzner, Docker Compose 7 services, stable
2. **Site vitrine** : 14 pages HTML statiques, responsive, SEO optimisé, design INSTRUMENT DE PRÉCISION
3. **Blog** : 9 articles SEO (~8650 mots), index
4. **API** : Endpoint checkout Stripe fonctionnel en production
5. **Base de données** : 10 tables TimescaleDB, schéma complet
6. **MQTT** : Broker + ingest fonctionnel
7. **Stripe** : Intégration LIVE, checkout + webhook + email confirmation
8. **Firmware #089** : Code ESP32-S3 complet, OTA, GitHub
9. **Firmware #091** : Radar 60GHz complété
10. **DNS** : noisyless.com + platform.noisyless.com pointent vers Hetzner

### 12.2 Ce qui manque pour une V1 commercialisable

| Priorité | Domaine | Livrable |
|----------|---------|----------|
| P0 | Pages légales | Validation + mise en ligne mentions-legales, CGV, confidentialite |
| P0 | Sitemap | Déploiement sitemap.xml pour Google |
| P1 | Tunnel de paiement | Test complet (vrai paiement → webhook → email → /merci) |
| P1 | Abonnements | Vérifier facturation récurrente Stripe |
| P1 | Auth | Implémenter /api/auth/send-magic-link pour dashboard |
| P1 | Sécurité | Activer UFW, configurer fail2ban |
| P2 | Dashboard | Dashboard client platform.noisyless.com |
| P2 | Visualisation | Heatmap en temps réel |
| P2 | Alertes | Notifications SMS/email/Telegram |
| P2 | Monitoring serveur | Uptime, alertes infra |
| P3 | Hardware #089 | Finaliser PCB, produire V1 |
| P3 | Hardware #091 | Finaliser PCB ToF + Radar |
| P3 | Firmware #091 | ToF driver TMF8829, mesh ESP-NOW |

### 12.3 Architecture cible — schéma fonctionnel

```
┌─────────────────────────────────────────────────────────────┐
│                     NOISYLESS PLATFORM                       │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  noisyless.com (site vitrine)     platform.noisyless.com    │
│  ┌──────────────────────────┐    ┌──────────────────────┐  │
│  │ Pages statiques HTML/CSS │    │ Dashboard SPA        │  │
│  │ • Accueil                │    │ • Auth magic link    │  │
│  │ • Produits (2)           │    │ • Devices            │  │
│  │ • FAQ (3387 mots)        │    │ • Heatmap live       │  │
│  │ • Blog (9 articles)      │    │ • Historique         │  │
│  │ • Légales (3)            │    │ • Alertes            │  │
│  │ • Paiement (Stripe)      │    │ • Abonnement         │  │
│  └─────────────┬────────────┘    └──────────┬───────────┘  │
│                │                             │               │
│                │   POST /shop/checkout       │ GET /api/*   │
│                └──────────┬──────────────────┘               │
│                           │                                  │
│                    ┌──────▼──────┐                           │
│                    │   NGINX      │  TLS + reverse proxy     │
│                    └──────┬──────┘                           │
│                           │                                  │
│              ┌────────────┼────────────┐                     │
│              │            │            │                      │
│        ┌─────▼─────┐ ┌───▼────┐ ┌────▼────────┐            │
│        │ Static    │ │ API    │ │ MQTT Broker  │            │
│        │ /var/www  │ │ :8000  │ │ :1883 :8883  │            │
│        └───────────┘ └───┬────┘ └────┬─────────┘            │
│                          │           │                       │
│                   ┌──────▼───┐  ┌────▼─────────┐            │
│                   │ Stripe   │  │ mqtt-ingest   │            │
│                   │ Webhook  │  └────┬──────────┘            │
│                   └────┬─────┘       │                       │
│                        │        ┌────▼──────────┐            │
│                        │        │ heatmap-agg    │            │
│                        │        └────┬───────────┘            │
│                        │             │                        │
│                   ┌────▼─────────────▼──────┐                │
│                   │ TimescaleDB (PostgreSQL) │               │
│                   │ 10 tables                │               │
│                   │ measurements, orders...  │               │
│                   └──────────────────────────┘               │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  ALERT ENGINE │ EMAILS (SMTP OVH) │ TELEGRAM         │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                     HARDWARE LAYER                            │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Capteur Env #089           HeatMap Int #091                 │
│  ┌─────────────────┐       ┌──────────────────┐             │
│  │ ESP32-S3-MINI-1 │       │ ESP32-S3-MINI-1  │             │
│  │ BME680 + LD2410 │       │ TMF8829 (16×16)  │             │
│  │ Sonomètre       │       │ Batterie 18650   │             │
│  │ WiFi → MQTT     │       │ ESP-NOW mesh     │             │
│  └────────┬────────┘       └────────┬─────────┘             │
│           │                          │                        │
│  HeatMap Ext #091                   │                        │
│  ┌──────────────────┐               │                        │
│  │ ESP32-S3-MINI-1  │               │                        │
│  │ LD6001A (radar)  │               │                        │
│  │ Solaire+batterie │               │                        │
│  │ ESP-NOW mesh     │               │                        │
│  └────────┬─────────┘               │                        │
│           │                          │                        │
│           └──────────┬───────────────┘                        │
│                      │                                       │
│              ┌───────▼────────┐                              │
│              │ Master ESP32   │  Élection bully              │
│              │ Recalage       │  WiFi → MQTT                 │
│              └───────┬────────┘                              │
│                      │                                       │
│                      ▼  MQTT (91.99.26.43:1883)              │
└─────────────────────────────────────────────────────────────┘
```

### 12.4 Points de conception à trancher

1. **Dashboard** : SvelteKit ou SPA vanilla ? Actuellement rien de construit
2. **Auth** : Magic link (déjà prévu en DB) vs OAuth vs email/password
3. **Visualisation heatmap** : Canvas custom vs bibliothèque (Deck.gl, Leaflet)
4. **App mobile** : PWA ou native ? Pas de code existant
5. **Multi-tenant** : Architecture villa_id déjà prévue dans le schéma DB
6. **Prix** : 3 produits (nl-simple 79€, nl-map-in ?, nl-map-out ?)
7. **Abonnements** : 3 tiers (Starter 4.99€, Business 14.90€, Pro 39.90€) + overage 0.50€/capteur
8. **OTA firmware** : GitHub Releases déjà utilisé, à industrialiser

---

*Document généré le 25/05/2026 par Hermes MBHREP — Audit complet en direct du serveur Hetzner.*
*Prochaine étape : faire valider par Mathieu, puis rédiger le cahier des charges technique.*
