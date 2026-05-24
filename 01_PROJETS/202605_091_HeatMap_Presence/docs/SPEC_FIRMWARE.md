# Firmware Specification — NOISYLESS HeatMap #091

> **Version** : 1.0-draft | **Date** : 2026-05-23 | **Auteur** : MBHREP/ELEC-CORE
> **Cible** : ESP32-S3-MINI-1 | **Framework** : ESP-IDF + FreeRTOS
> **North Star** : SaaS récurrent par villa → revenu, pas marge hardware. Tout cycle CPU inutile = coût différé.

---

## 1. Architecture logicielle

```
┌──────────────────────────────────────────────────────────┐
│                   APPLICATION LAYER                       │
│  ┌──────────┐  ┌───────────┐  ┌──────────────────────┐   │
│  │  Task    │  │  Task     │  │  Task                │   │
│  │  Sensor  │  │  Mesh     │  │  Master (conditionnel)│   │
│  │  (driver)│  │  (ESP-NOW)│  │  Fusion spatiale     │   │
│  └────┬─────┘  └─────┬─────┘  └──────────┬───────────┘   │
│       │              │                    │               │
│  ┌────┴──────────────┴────────────────────┴───────────┐   │
│  │              Event Bus / Message Queue              │   │
│  └────────────────────────┬───────────────────────────┘   │
│                           │                               │
│  ┌────────────────────────┴───────────────────────────┐   │
│  │                  SERVICES LAYER                     │   │
│  │  Power Mgr  │  OTA  │  Health  │  Config  │  Log    │   │
│  └────────────────────────────────────────────────────┘   │
│                           │                               │
│  ┌────────────────────────┴───────────────────────────┐   │
│  │                  HAL / DRIVERS                      │   │
│  │  I2C (TMF8829) │ UART (LD6001A) │ GPIO │ ADC │ RTC │   │
│  └────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────┘
```

**Principe** : même binaire pour tous les nodes. Le rôle (ToF / Radar / Master) est déterminé au boot par la config stockée en NVS.

---

## 2. Tâches FreeRTOS

### 2.1 Table des tâches

| Tâche | Priorité | Stack | Core | Période | Rôle |
|-------|----------|-------|------|---------|------|
| `sensor_task` | 3 | 4096 | 0 | Déclenchée par timer (période configurable) | Lecture capteur + formatage payload |
| `mesh_task` | 4 | 6144 | 1 | Event-driven (ESP-NOW callback) | Réception/émission mesh, master election |
| `master_task` | 2 | 8192 | 0 | Déclenchée sur réception payload complet | Recalage spatial + fusion → heatmap |
| `mqtt_task` | 3 | 4096 | 1 | Event-driven | MQTT TX (heatmap finale → AOS-Core) |
| `health_task` | 1 | 2048 | 0 | 30s | Watchdog, tension batterie, status LED |
| `ota_task` | 1 | 4096 | 0 | Déclenché sur notification | OTA SHA-256 |

### 2.2 sensor_task — commun ToF / Radar

```
boucle:
  wait(sensor_timer_semaphore)  // donné par timer HW ou duty-cycle
  
  si ROLE == TOF:
    zones = tmf8829_read_16x16()
    payload = {
      type: "tof",
      device_id: MAC,
      ts: rtc_now(),
      zones: [[dist_cm_0..15] * 16],
      confidence_pct: avg_confidence()
    }
  
  si ROLE == RADAR:
    targets = ld6001a_read_tracks()
    payload = {
      type: "radar",
      device_id: MAC,
      ts: rtc_now(),
      targets: [{x, y, z, v, confidence}],
      count: len(targets)
    }
  
  mesh_send(payload)  // unicast au master élu
  power_down_sensor()  // duty-cycle agressif si batterie
```

### 2.3 mesh_task — ESP-NOW

**Protocole mesh simplifié ESP-NOW :**

```
Trame ESP-NOW (max 250 bytes) :
┌──────┬──────┬────────┬──────────┬────────────────────┐
│ Type │  ID  │ Master │   TTL    │     Payload        │
│  1B  │  6B  │  1B    │   1B     │     max 241 bytes  │
└──────┴──────┴────────┴──────────┴────────────────────┘

Types :
  0x01 DATA       → payload capteur (ToF ou Radar)
  0x02 HEARTBEAT  → alive + battery + role
  0x03 ELECTION   → master election (bully algorithm)
  0x04 HEATMAP    → heatmap finale (master → broker bridge)
  0x05 ACK        → acquittement
```

**Master election — Bully simplifié :**

1. Au boot, chaque node écoute 2s.
2. Si aucun `HEARTBEAT` avec `master=true` → élection.
3. Le node avec l'adresse MAC la plus basse gagne.
4. Master broadcast `HEARTBEAT(master=true)` toutes les 5s.
5. Si master silencieux > 15s → réélection.
6. Master = celui qui collecte + fusionne + transmet MQTT.

**Payload DATA → mesh (max 241 bytes) :**

```json
{
  "t": 1,                    // type DATA
  "d": "a1b2c3d4e5f6",      // device_id (MAC)
  "r": 0,                    // role: 0=TOF, 1=RADAR
  "ts": 1716472800,          // timestamp RTC
  "b": 3.7,                  // tension batterie
  "p": {                     // payload capteur (compressé)
    "z": [[b64_encoded]],    // zones ToF (diff encoding)
    "c": 85                  // confidence %
  }
}
```

**Payload HEATMAP → master → MQTT (final, pas mesh) :**

```json
{
  "villa_id": "villa_001",
  "ts": 1716472800,
  "node_count": 12,
  "threshold_breach": true,
  "total_count": 15,
  "max_count": 8,
  "heatmap": {
    "resolution": "1m",
    "grid": [[x, y, count], ...],
    "dimensions": {"w": 20, "h": 30}
  },
  "zones": {
    "salon": {"count": 8, "trend": "rising"},
    "terrasse": {"count": 5, "trend": "stable"},
    "piscine": {"count": 2, "trend": "stable"}
  }
}
```

---

## 3. Master — Recalage spatial & fusion

Le master tourne sur son propre ESP32-S3. Opérations :

```
1. Réception payload de tous les nodes (window glissante 2s)
2. Recalage spatial :
   - Charger calibration pré-enregistrée : {device_id → (x, y, θ, FOV, portée)}
   - Transformer chaque grille ToF et chaque cible radar dans le repère global
3. Fusion ToF + Radar :
   - Zones intérieures : grille ToF 16×16 → maillage villa (interpolation bilinéaire)
   - Zones extérieures : points radar → grille villa (accumulation gaussienne)
   - Zone overlap (transition int/ext) : pondération linéaire selon distance bordure
4. Comptage :
   - Density-based clustering (DBSCAN simplifié, ε=0.5m, min_samples=2)
   - count = nombre de clusters
5. Heatmap finale → MQTT compressé (msgpack ou JSON minifié)
6. Alerte locale si count > seuil (buzzer? LED?), même sans connexion backend
```

**Recalage spatial — détail :**

```
Coordonnées monde :
  x_world = x_node + (x_local * cos(θ) - y_local * sin(θ))
  y_world = y_node + (x_local * sin(θ) + y_local * cos(θ))

Pour le ToF (grille 16×16) :
  - Chaque cellule (i,j) couvre un rectangle au sol
  - À H=3m, FOV 67.9°×52.8° → couverture ~4.2m × 3.0m
  - cellule ~0.26m × 0.19m

Pour le Radar (nuage de points) :
  - Chaque cible = (r, θ, φ) → (x, y, z) monde via pose du capteur
```

**Calibration stockée en NVS :** `calib` namespace. Format JSON dans une clé unique :
```json
{
  "a1b2c3d4e5f6": [2.5, 1.0, 0, 67.9, 5.0, 0],  // x, y, θ, FOV_h, FOV_v, portée
  "b2c3d4e5f6a1": [5.0, 1.0, 90, null, null, 12.0, 1]  // radar: x, y, θ, null, null, portée, type
}
```

---

## 4. Drivers — TMF8829 (I²C)

### 4.1 Init

```
I2C: 400 kHz fast mode
Adresse 7-bit : 0x29 (TMF8829 après power-up)
Config requise :
  - Mode 16×16 (register 0x14 → 0x01)
  - ITERATIONS = 500 (per datasheet, compromis portée/précision)
  - Période ranging = 33 ms (30 Hz max)
  - Seuil confidence = 5 (valeur datasheet par défaut)
```

### 4.2 Lecture

```
1. Écrire commande START (0x80 → reg 0x00)
2. Poll reg 0x01 → attendre bit READY (timeout 67ms)
3. Lire résultat : 16×16×2 bytes = 512 bytes en burst I²C
4. Décoder : chaque zone = uint16 (distance_mm), confidence dans octet de poids faible
5. Post-processing côté MCU : remplacer valeurs hors range (>5000 ou 0) par -1
```

### 4.3 Cycle batterie

```
Éveil complet : ~30 ms (PLL lock)
Ranging actif : 117 mA × 33ms = 3.86 mAs/lecture
Standby entre lectures : 65 µA
Transmission WiFi : ~80 µAh par TX

Exemple budget 1×18650 (3000 mAh) :
  - Lecture toutes les 60s → 1440 lectures/jour
  - Ranging : 1440 × 3.86 mAs = 5.56 mAh/jour
  - Standby : 23.99h × 0.065 mA = 1.56 mAh/jour
  - WiFi TX (10×/jour sur delta) : 10 × 0.08 mAh = 0.8 mAh/jour
  - Total : ~7.9 mAh/jour → ~380 jours autonomie
```

---

## 5. Drivers — LD6001A (UART)

### 5.1 Interface

```
UART : 115200 bauds, 8N1
Protocole : trames binaires (pas AT commands, protocole constructeur)
Format trame (hypothèse standard radar mmWave) :
  Header 0x55 0xAA | Length 2B | Type 1B | Payload N bytes | CRC 2B

Type 0x01 = cibles tracking
Type 0x02 = configuration ACK
Type 0x03 = status

Payload cible unique (12 bytes) :
  target_id 1B | x 2B (cm, int16) | y 2B | z 2B | velocity 2B (cm/s) | confidence 1B
```

### 5.2 Config

```
Via UART à l'init :
  1. Détection baudrate automatique (envoyer 0x55 0xAA 0x00 0x00)
  2. Lire version firmware
  3. Configurer seuil détection (sensibilité) : register SENSITIVITY = 5/10
  4. Configurer zone morte : < 50 cm ignoré (rebonds proches)
  5. Mode : continuous tracking, rapport toutes les 100ms
```

### 5.3 Consommation

```
Actif : ~1.5W (estimé module 60 GHz)
Solaire : panneau 5W + batterie 5000 mAh LiFePO4
3 jours autonomie : 5000 mAh × 3.2V = 16 Wh / 4.5 Wh/jour (1.5W × 3h peak tracking) = 3.5j
→ OK, avec marge si tracking intermittent
```

---

## 6. Power Management

### 6.1 ToF (batterie 18650)

| État | Action | Courant | Durée typique |
|------|--------|---------|---------------|
| `SLEEP` | deep sleep ESP32, TMF8829 power-down | 2 µA + 8 µA RTC | 58s (sur cycle 60s) |
| `WAKE` | boot ESP32 (fast wake from RTC) | 40 mA | 200 ms |
| `RANGING` | TMF8829 16×16 actif | 117 mA + 80 mA CPU | 33 ms |
| `PROCESS` | CPU actif, I²C burst, format payload | 80 mA | 5 ms |
| `TX` | ESP-NOW émission (si delta détecté) | 300 mA | 3 ms |
| `STANDBY` | retour attente (si heartbeat à envoyer) | 65 µA + 20 mA CPU light sleep | variable |

**Algorithme décision TX :**
```
si |count_current - count_previous| > 1 OU time_since_last_tx > 120s:
  mesh_send(payload)
  previous_count = current_count
```

### 6.2 Radar (solaire + batterie)

| État | Action | Courant |
|------|--------|---------|
| `ACTIVE_DAY` | LD6001A tracking continu + CPU | ~1.5W |
| `IDLE_NIGHT` | LD6001A sleep, CPU deep sleep, réveil /5min | 50 µA total |
| `ACTIVE_NIGHT` | Si alerte occupée → reprise tracking | 1.5W |
| `CHARGING` | Panneau solaire actif → charge batterie | — |

---

## 7. OTA

```
Mécanisme : double partition OTA (factory + ota_0 / ota_1)
Source : manifest JSON sur GitHub (mberthod/noisyless-heatmap-firmware)
Vérification : SHA-256 du binaire avant flash

Manifest :
{
  "version": "1.0.1",
  "url": "https://github.com/mberthod/noisyless-heatmap-firmware/releases/download/v1.0.1/heatmap-fw.bin",
  "sha256": "ab12...",
  "min_version": "1.0.0",
  "target_roles": ["all"]
}

Déclencheur : mqtt topic noisyless/{villa_id}/ota/notify → {version, url, sha256}
```

---

## 8. Config NVS

| Namespace | Clé | Type | Valeur par défaut |
|-----------|-----|------|-------------------|
| `identity` | `role` | u8 | 0xFF (auto-detect) |
| `identity` | `device_id` | string | MAC |
| `calib` | `pose` | string (JSON) | null |
| `calib` | `villa_id` | string | "unassigned" |
| `sensor` | `period_ms` | u32 | 60000 (ToF), 1000 (Radar) |
| `sensor` | `duty_cycle_s` | u32 | 60 |
| `power` | `battery_low_mv` | u32 | 3200 |
| `master` | `fusion_window_ms` | u32 | 2000 |
| `master` | `threshold_count` | u8 | 10 |
| `mesh` | `channel` | u8 | 1 |
| `mqtt` | `broker_uri` | string | "mqtts://192.168.0.176:8883" |

---

## 9. Logging & Debug

```
UART0 : logs série (115200, 8N1)
Format : [LEVEL][TASK_NAME] message
Niveaux : ERROR, WARN, INFO, DEBUG

LED RGB (actif bas) :
  ROUGE   = erreur (batterie basse, capteur HS)
  VERT    = heartbeat OK (clignotement 1s)
  BLEU    = master actif
  JAUNE   = élection en cours
  BLANC   = OTA en cours

Health endpoint (MQTT, toutes les 30s si master) :
{
  "device_id": "a1b2...",
  "role": "master",
  "uptime_s": 86400,
  "battery_mv": 3700,
  "free_heap": 123456,
  "mesh_peers": 11,
  "last_sensor_ms": 1234,
  "error_count": 0
}
```

---

## 10. Failsafes

| Scénario | Comportement |
|----------|-------------|
| Capteur I²C timeout (>100ms) | Reset I²C bus, 3 tentatives, puis alerte ERROR |
| Master down (>15s sans heartbeat) | Réélection automatique |
| WiFi AP down | Buffer circulaire 100 entrées en RTC RAM, TX en rafale au retour |
| Batterie < 3.2V | Deep sleep 10 min, TX LOW_BATTERY alert, puis extinction propre |
| OTA échouée (SHA mismatch) | Rollback partition factory |
| MQTT déconnecté > 5 min | Reconnect exponential backoff (1s, 2s, 4s... max 60s) |
| Watchdog task bloquée | HW watchdog 10s → reset complet |

---

## 11. Roadmap firmware

| Phase | Livrable | Effort |
|-------|----------|--------|
| **P1.1** | Driver TMF8829 I²C + lecture 16×16 + payload | 3j |
| **P1.2** | Mesh ESP-NOW : découverte + HEARTBEAT + élection | 5j |
| **P1.3** | Master : collecte + recalage spatial + fusion | 5j |
| **P1.4** | Power mgmt : duty-cycle ToF + deep sleep | 3j |
| **P2.1** | Driver LD6001A UART + parsing cibles | 3j |
| **P2.2** | Intégration mesh radar + fusion ToF/radar combiné | 3j |
| **P3.1** | MQTT bridge master → AOS-Core + compression payload | 2j |
| **P4** | OTA + config NVS + calibration tooling | 3j |
| **P5** | Tests villa réelle + itération | 5j |

**Total estimé : 32 jours-homme firmware**
