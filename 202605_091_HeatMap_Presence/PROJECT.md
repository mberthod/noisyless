# NOISYLESS HeatMap — Projet #091

## Métadonnées

| Champ | Valeur |
|---|---|
| **N° projet AOS-Core** | #091 |
| **Slug** | `202605_Projet_091_NOISYLESS_HeatMap` |
| **Date création** | 2026-05-22 |
| **Entité** | NOISYLESS |
| **Phase actuelle** | `conception` — étude préliminaire |
| **Lifecycle state** | `active` |
| **Budget estimé** | À chiffrer après pilote |
| **Prochain jalon** | Signature client pilote + gel specs extérieur/intérieur |
| **Contact entité** | `contact@noisyless.com` |
| **Kdrive path** | `01_CLIENTS/NOISYLESS/NOISYLESS_heatmap/` |

---

## 1. Identité & Objectif

**Sous-projet de NOISYLESS**. Objectif : **compter les personnes dans une grande villa (intérieur + extérieur) → heatmap de densité**.

- **Client** : en négociation, cible = pilote payant
- **Usage réel probable** : détection de **suroccupation / fête** en location courte durée haut de gamme
- **Pas du comptage métrologique** — on détecte des patterns, pas des chiffres exacts
- **Revenu cible** : SaaS récurrent par villa (pas la marge hardware)
- **North Star** : contribue aux +200 000 € perso d'ici déc. 2026 si le pilote convertit

---

## 2. Règle d'or technique

> **Le ToF TMF8829 ne marche pas en extérieur diurne** (plein soleil → portée ~0,6 m).

→ Architecture **hétérogène obligatoire** :

| Zone | Capteur | Contrôleur | Montage |
|---|---|---|---|
| **Intérieur** | TMF8829 (mode 16×16) | ESP32-S3 | Plafond |
| **Extérieur** | Radar mmWave 60 GHz (TI IWR6843AOP / IWRL6432) | ESP32-S3 ou TI SoC | Mural/plafond |
| **Backend** | Agrégation MQTT → recalage spatial → heatmap + alertes | AOS-Core (192.168.0.176) | — |

**RGPD-clean** : aucun nœud ne produit d'image identifiante (ToF = grille de distances, radar = points 3D).

---

## 3. Architecture

```
┌─────────────────────────┐    ┌──────────────────────────┐
│  Node TOF (intérieur)   │    │  Node Radar (extérieur)  │
│  ESP32-S3 + TMF8829     │    │  ESP32-S3 + IWR6843AOP  │
│  16×16 zones, plafond   │    │  60 GHz, insensible     │
│  Secteur (temps réel)   │    │  lumière du jour        │
└──────────┬──────────────┘    └───────────┬──────────────┘
           │                               │
           └─────── WiFi MQTT TLS ─────────┘
                        │
           ┌────────────▼────────────┐
           │  AOS-Core (192.168.0.176)│
           │  ingest-mqtt            │
           │  → recalage spatial     │
           │  → heatmap-agg          │
           │  → alertes seuil        │
           │  → dashboard client     │
           └─────────────────────────┘
```

### 3.1 Comptage & Énergie

- **Bufferiser** les comptages → transmettre **rarement** (WiFi TX ~80 µAh domine le budget énergie)
- **Intérieur temps réel** → secteur (ToF scanne en continu)
- **Batterie réservée** aux nœuds présence-lente isolés (pas de scanning continu)
- **Pas de monitoring bruit** dans ce SKU — incompatible batterie, c'est l'autre produit NOISYLESS

---

## 4. Roadmap

| Phase | Priorité | Contenu | Livrable |
|---|---|---|---|
| **P0** | 🔴 Immédiat | Verrouiller avec le client : (1) scope extérieur, (2) seuil vs comptage exact, (3) exclusion ToF extérieur diurne **par écrit** | Contrat/CDC signé |
| **P1** | 🟠 Proto | Node TOF intérieur : ESP32-S3 + TMF8829, firmware MQTT, boîtier plafond | 1 proto fonctionnel |
| **P2** | 🟠 Proto | Node Radar extérieur : IWR6843AOP/IWRL6432, SDK TI, intégration MQTT | 1 proto fonctionnel |
| **P3** | 🟡 Backend | ingest-mqtt + recalage spatial + heatmap-agg + alertes seuil sur AOS-Core | Services Docker |
| **P4** | 🟡 Dashboard | Extension dashboard NOISYLESS existant : heatmap + alertes | Onglet client |
| **P5** | 🟢 Pilote | Déploiement villa pilote, tests, itération | Retour client |

---

## 5. Spécifications techniques

### 5.1 Node TOF Intérieur (ESP32-S3 + TMF8829)

| Élément | Spéc |
|---|---|
| Capteur | AMS TMF8829 (dToF 8×8 ou 16×16 zones) |
| Mode | 16×16 zones, portée utile intérieure ~5 m |
| MCU | ESP32-S3 (WiFi + BLE) |
| Firmware | ESP-IDF / PlatformIO, FreeRTOS |
| Com | MQTT TLS (Mosquitto → AOS-Core) |
| Alim | Secteur USB-C (temps réel) |
| Payload | `{zones: [[dist_cm]], confidence: %, ts, device_id}` |
| Fréquence TX | 1×/30s ou sur delta significatif |
| Montage | Plafond, fixation standard |

### 5.2 Node Radar Extérieur (IWR6843AOP/IWRL6432 + ESP32-S3)

| Élément | Spéc |
|---|---|
| Capteur | TI IWR6843AOP (60 GHz, antenne on-package) ou IWRL6432 (basse conso) |
| Portée | Jusqu'à 30 m extérieur, insensible lumière |
| MCU | ESP32-S3 (WiFi + BLE) ou TI SoC intégré |
| Firmware | SDK TI mmWave + ESP-IDF pont UART |
| Com | MQTT TLS (Mosquitto → AOS-Core) |
| Alim | Secteur ou PoE |
| Payload | `{targets: [{x, y, z, v}], count, ts, device_id}` |
| Fréquence TX | 1×/10s |

### 5.3 Backend (AOS-Core, 192.168.0.176)

| Service | Rôle |
|---|---|
| `ingest-mqtt` | Souscrit `noisyless/+/heatmap`, écrit dans TimescaleDB |
| `heatmap-agg` | Recalage spatial (plan villa → zones), agrégation rolling 5 min |
| `alert-engine` | Seuils configurable : count > N → alerte Telegram/Dashboard |
| Dashboard | Extension SvelteKit existant, nouvel onglet « Heatmap » |

---

## 6. Garde-fous (tous les agents)

1. **Ne jamais proposer du ToF pour couvrir un jardin/terrasse de jour** — ça ne marche pas
2. **Ne jamais dimensionner un nœud bruit-continu sur batterie** — pas dans ce SKU
3. **Toujours rattacher une tâche à une phase de la roadmap (P0→P5) et au filtre North Star**
4. **Si une demande dérive vers du « framework gratuit »** (refonte orchestrateur, infra non vendable) → le signaler immédiatement

---

## 7. Avant signature client (P0 bloquant)

- [ ] Scope extérieur verrouillé (surfaces, nombre de nœuds radar)
- [ ] Accord écrit : **détection de seuil**, pas comptage exact
- [ ] Exclusion ToF extérieur diurne documentée + acceptée
- [ ] Prix pilote validé (SaaS mensuel par villa)
