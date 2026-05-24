# NOISYLESS — Organisation des projets

**Date :** 24 Mai 2026  
**Vue d'ensemble :** 3 produits distincts, 1 infrastructure commune

---

## 📋 Les 3 projets hardware

| Projet | Code | Capteurs | Usage | Prix cible |
|--------|------|----------|-------|------------|
| **NOISYLESS Simple** | `nl-presence` | PIR + ToF mono + CO₂ + Son | Détection présence basique | ~50 € |
| **NOISYLESS HeatMap Intérieur** | `nl-map-in` | Matrice ToF 16×16 (TMF8829) | Heatmap intérieure | ~80 € |
| **NOISYLESS HeatMap Extérieur** | `nl-map-out` | Radar 60 GHz (MS72SF1) | Heatmap extérieure | ~120 € |

---

## 🏗️ Architecture commune

```
┌─────────────────────────────────────────────────────────────┐
│                    NOISYLESS PLATFORM                       │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                  │
│  │  Simple  │  │   Int    │  │   Ext    │                  │
│  │ nl-      │  │ nl-map-  │  │ nl-map-  │                  │
│  │ presence │  │   in     │  │   out    │                  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘                  │
│       │             │             │                         │
│       └─────────────┴─────────────┘                         │
│                     │                                       │
│                     ▼                                       │
│              ┌─────────────┐                                │
│              │   Mosquitto │  MQTT Broker                   │
│              │   (TLS 8883)│                                │
│              └──────┬──────┘                                │
│                     │                                       │
│                     ▼                                       │
│              ┌─────────────┐                                │
│              │  TimescaleDB │  Base de données              │
│              └──────┬──────┘                                │
│                     │                                       │
│       ┌─────────────┼─────────────┐                        │
│       │             │             │                         │
│       ▼             ▼             ▼                         │
│  ┌────────┐   ┌────────┐   ┌────────┐                      │
│  │ ingest │   │  agg   │   │ alert  │                      │
│  │  MQTT  │   │heatmap │   │ engine │                      │
│  └────────┘   └────────┘   └────────┘                      │
│                                                             │
│       ┌─────────────────────────────┐                      │
│       │      FastAPI Backend        │                      │
│       │   + Dashboard Web/Mobile    │                      │
│       └─────────────────────────────┘                      │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 📁 Arborescence des répertoires

```
/home/mathieu/Kdrive/01_CLIENTS/NOISYLESS/
├── 00_INFOS_CLIENT/              # Contrat, factures, contacts
│
├── _infra/                       # INFRASTRUCTURE COMMUNE
│   ├── docker-compose.yml        # Tous services
│   ├── services/
│   │   ├── mqtt-ingest/          # Subscribe MQTT → DB
│   │   ├── heatmap-agg/          # Agrégation 5min
│   │   ├── alert-engine/         # Alertes Telegram
│   │   └── api/                  # FastAPI REST
│   ├── mosquitto/                # Config broker
│   ├── timescaledb/              # Config DB
│   ├── nginx/                    # Reverse proxy + SSL
│   └── docs/
│       ├── DEPLOYMENT.md         # Guide déploiement (SIMPLE !)
│       ├── MAINTENANCE.md        # Opérations courantes
│       └── TROUBLESHOOTING.md    # Problèmes courants
│
├── 202605_091_HeatMap_Presence/  # PROJET PILOTE ( HeatMap )
│   ├── PROJECT.md                # Fiche projet #091
│   │
│   ├── 01_NOISYLESS_SIMPLE/      # Produit : Détection basique
│   │   ├── firmware/
│   │   │   └── node-presence/
│   │   │       ├── src/
│   │   │       ├── platformio.ini
│   │   │       └── README.md
│   │   ├── hardware/
│   │   │   ├── schematics/
│   │   │   └── bom/
│   │   ├── docs/
│   │   │   ├── QUICKSTART.md     # Démarrage rapide
│   │   │   └── INSTALL.md        # Installation
│   │   └── scripts/
│   │       └── flash.sh
│   │
│   ├── 02_NOISYLESS_HEATMAP_INT/ # Produit : Heatmap intérieur
│   │   ├── firmware/
│   │   │   └── node-tof-esp32s3/
│   │   │       ├── src/
│   │   │       ├── components/
│   │   │       ├── platformio.ini
│   │   │       └── README.md
│   │   ├── hardware/
│   │   │   ├── schematics/
│   │   │   └── bom/
│   │   ├── docs/
│   │   │   ├── QUICKSTART.md
│   │   │   └── INSTALL.md
│   │   └── scripts/
│   │       └── flash.sh
│   │
│   └── 03_NOISYLESS_HEATMAP_EXT/ # Produit : Heatmap extérieur
│       ├── firmware/
│       │   └── node-radar-ms72sf1/
│       │   ├── src/
│       │   ├── platformio.ini
│       │   └── README.md
│       ├── hardware/
│       │   ├── schematics/
│       │   └── bom/
│       ├── docs/
│       │   ├── QUICKSTART.md
│       │   └── INSTALL.md
│       └── scripts/
│           └── flash.sh
│
└── 202604_089_Capteur_Environnemental_V1/  # Ancien projet (archive)
```

---

## 🎯 Différences entre projets

### NOISYLESS Simple (`nl-presence`)

**Objectif :** Détecter présence + bruit + qualité d'air

**Capteurs :**
- PIR (présence binaire)
- ToF mono-zone (distance)
- SCD4x (CO₂, température, humidité)
- INMP441 (niveau sonore dB)

**Firmware :**
- Publish toutes les 30s
- Payload simple : `{present: bool, co2: int, db: int, temp: float}`
- Batterie possible (deep sleep)

**Backend :**
- Table `measurements` standard
- Alertes seuil CO₂ / bruit

---

### NOISYLESS HeatMap Intérieur (`nl-map-in`)

**Objectif :** Heatmap de densité intérieure

**Capteurs :**
- TMF8829 (ToF 16×16 zones)
- ESP32-S3 (WiFi + BLE)

**Firmware :**
- Mesh ESP-NOW (multi-noeuds)
- Master → MQTT toutes les 10s
- Payload : `{grid: [[count]], total: int}`

**Backend :**
- Continuous aggregates 5min
- Heatmap par zone
- Alertes suroccupation

---

### NOISYLESS HeatMap Extérieur (`nl-map-out`)

**Objectif :** Heatmap de densité extérieure

**Capteurs :**
- MS72SF1 (Radar 60 GHz)
- ESP32-S3 (WiFi + BLE)

**Firmware :**
- UART parsing (trames binaires TLV)
- Mesh ESP-NOW (multi-noeuds)
- Master → MQTT toutes les 10s
- Payload : `{targets: [{x,y,z,v}], count: int}`

**Backend :**
- Agrégation cibles 3D
- Heatmap projetée 2D
- Alertes suroccupation

---

## 🚀 Démarrage rapide

### 1. Déployer l'infrastructure

```bash
cd /home/mathieu/Kdrive/01_CLIENTS/NOISYLESS/_infra
./scripts/deploy.sh
```

→ Voir `_infra/docs/DEPLOYMENT.md`

### 2. Flasher un device

```bash
cd 01_NOISYLESS_SIMPLE/firmware/node-presence
pio run --target upload
```

→ Voir `01_NOISYLESS_SIMPLE/docs/QUICKSTART.md`

### 3. Claimer un device

```bash
curl -X POST https://platform.noisyless.com/api/devices/claim \
  -H "Authorization: Bearer <token>" \
  -d '{"duid": "24:EC:4A:23:36:BC", "claim_code": "HMAC..."}'
```

---

## 📊 État d'avancement

| Projet | Firmware | Hardware | Backend | Docs | Statut |
|--------|----------|----------|---------|------|--------|
| **Simple** | 🟡 50% | ⚪ 0% | ✅ 80% | 🟡 50% | En dev |
| **HeatMap Int** | ✅ 90% | ⚪ 0% | ✅ 80% | 🟡 50% | Pilote |
| **HeatMap Ext** | 🟡 30% | ⚪ 0% | 🟡 50% | 🟡 50% | En dev |
| **Infra** | N/A | N/A | ✅ 90% | 🟡 60% | quasi-OK |

---

## 🔗 Liens utiles

- **Infrastructure :** `_infra/docs/DEPLOYMENT.md`
- **Projet #091 :** `PROJECT.md`
- **Architecture :** `_infra/ARCHITECTURE.md`
- **Migration :** `_infra/MIGRATION_PLAN.md`

---

*Document créé : 24 Mai 2026*  
*Mis à jour : 24 Mai 2026*
