# NOISYLESS — Site Inventory (audit 25/05/2026)

> Audit complet en direct du serveur Hetzner 91.99.26.43.
> Source de référence pour tout agent intervenant sur la plateforme.

## Infrastructure

| Élément | Valeur |
|---------|--------|
| Serveur | Hetzner CAX11, Ubuntu 24.04 |
| IP | 91.99.26.43 |
| SSH | `noisyless@91.99.26.43` |
| Domaines | `noisyless.com` + `platform.noisyless.com` → 91.99.26.43 |
| DNS | Cloudflare proxy orange |
| Nginx config | `/opt/noisyless/nginx/nginx.conf` |
| Static files | `/opt/noisyless/static/` → monté `/var/www/static` (ro) |
| Env vars | `/opt/noisyless/.env` |

## Docker Compose — 7 conteneurs

```
noisyless-nginx          nginx:alpine            80, 443
noisyless-api            FastAPI uvicorn         :8000 (interne)
noisyless-timescaledb    TimescaleDB PG16        :5432 (127.0.0.1)
noisyless-mosquitto      eclipse-mosquitto:2     1883, 8883
noisyless-mqtt-ingest    Python                  interne
noisyless-heatmap-agg    Python                  interne
noisyless-alert-engine   Python                  interne
```

## Pages statiques déployées (/opt/noisyless/static/)

| URL | Taille | Mots | Titre | HTTPS |
|-----|--------|------|-------|-------|
| `/` index.html | 28.4 KB | 1943 | NOISYLESS \| Instrument de surveillance environnementale | 200 |
| `/merci.html` | 2.6 KB | 212 | Merci pour votre commande \| NOISYLESS | 200 |
| `/annule.html` | 3.8 KB | 319 | Paiement annulé \| NOISYLESS | 200 |
| `/faq.html` | 33.0 KB | 3387 | FAQ - NOISYLESS \| Détecteur de bruit Airbnb | 200 |
| `/boutique.html` | 12.8 KB | 981 | Boutique NOISYLESS — Acheter votre détecteur de bruit | 200 |
| `/pilote.html` | 11.6 KB | 858 | Demander un pilote \| NOISYLESS | 200 |
| `/mentions-legales.html` | 4.7 KB | 399 | Mentions légales \| NOISYLESS | 200 |
| `/confidentialite.html` | 6.0 KB | 526 | Politique de confidentialité \| NOISYLESS | 200 |
| `/cgv.html` | 6.0 KB | 589 | Conditions générales de vente \| NOISYLESS | 200 |
| `/produits/noisyless-simple.html` | 28.4 KB | 2004 | NOISYLESS Simple \| Capteur environnemental | 200 |
| `/produits/heatmap.html` | 13.5 KB | 1026 | NOISYLESS HeatMap \| Cartographie de présence | 200 |
| `/blog/` index.html | 7.8 KB | 678 | Blog \| NOISYLESS | 200 |
| `/llms.txt` | 763 B | — | — | 200 |
| `/robots.txt` | 321 B | — | — | 200 |

## Blog — 9 articles

| Fichier | Mots | Sujet |
|---------|------|-------|
| detecteur-bruit-airbnb-legal.html | 1108 | Légalité détecteurs bruit (2026) |
| minut-vs-noisyless-comparatif.html | 1037 | Minut vs NOISYLESS |
| nuisances-sonores-copropriete.html | 923 | Nuisances copropriété |
| rgpd-capteurs-location-conformite.html | 931 | RGPD capteurs location |
| suroccupation-location-courte-duree.html | 927 | Suroccupation |
| detecter-fete-airbnb-sans-camera.html | 921 | Détection fête sans caméra |
| vapotage-cigarette-location-detection.html | 920 | Vapotage/cigarette |
| degat-des-eaux-vanne-coupure-automatique.html | 916 | Dégât des eaux |
| channel-managers-monitoring.html | 876 | Channel managers |

## API — Endpoints testés

| Méthode | Route | Statut | Notes |
|---------|-------|--------|-------|
| POST | `/shop/checkout` | ✅ 200 | Payload: `{items, email, name}` → retourne `{session_id, url}` |
| GET | `/api/health` | ❌ 404 | Route non implémentée (utiliser `/health` sans `/api/`) |
| GET | `/api/auth/send-magic-link` | ❌ 404 | Route non implémentée |
| POST | `/shop/webhook` | — | Non testable en GET |

## Base de données — 10 tables

```
users              → comptes (email, password_hash, account_type perso/pro)
auth_tokens        → magic link (token_hash, expires_at, used)
devices            → devices (device_id, type, firmware, villa_id)
user_devices       → liaison user↔device
measurements       → télémétrie (26 colonnes, TimescaleDB hypertable)
heatmap_agg_5min   → agrégation spatiale 5 min
orders             → commandes Stripe (session_id, amount, status)
order_items        → lignes commande (product_id, quantity, unit_amount)
alert_thresholds   → seuils configurables
device_health      → santé devices
```

## Design System

| Élément | Valeur |
|---------|--------|
| Direction | INSTRUMENT DE PRÉCISION (Dieter Rams / Braun) |
| Titres | Space Mono |
| Corps | IBM Plex Sans |
| Fond | #FFFFFF |
| Texte | #1A1A1A |
| Accent | #FF6B00 (ambre, ≤5%) |
| Anti-patterns | Pas de dégradé, dark theme, emojis, blobs, glow, pill buttons, SaaS générique |

## Produits Stripe

| ID | Nom | Prix |
|----|-----|------|
| nl-simple | NOISYLESS Simple | 79€ |
| nl-map-in | NOISYLESS HeatMap Int | précommande |
| nl-map-out | NOISYLESS HeatMap Ext | précommande |
| nl-leak | NOISYLESS Leak | précommande |

## Abonnements

| Plan | Prix/mois | Capteurs |
|------|-----------|----------|
| starter | 4.99€ | 1-5 |
| business | 14.90€ | 6-20 |
| pro | 39.90€ | 21-100 |
| overage | 0.50€/capteur | >100 |

## Fichiers de référence dans Kdrive

```
Kdrive/01_CLIENTS/NOISYLESS/
├── 00_INFOS_CLIENT/
│   ├── Contacts.md
│   └── CAHIER_RECETTE_NOISYLESS.md        ← audit complet 25/05/2026
├── 01_PROJETS/
│   ├── 202604_089_Capteur_Environnemental_V1/  ← #089 HW/FW
│   └── 202605_091_HeatMap_Presence/            ← #091 HW/FW
├── _infra/                                     ← docker-compose, nginx, services
└── _admin/emails/
```
