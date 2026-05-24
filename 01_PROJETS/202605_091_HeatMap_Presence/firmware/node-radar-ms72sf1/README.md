# MS72SF1 Radar Node — Documentation

## Vue d'ensemble

**Capteur :** MinewSemi MS72SF1 (mmWave 60-64 GHz)  
**Usage :** Détection de présence + tracking multi-cibles extérieur/intérieur  
**Portée :** Jusqu'à 30 m, insensible lumière du jour  
**Interface :** UART (115200 baud) → Protocole binaire TLV

---

## Spécifications techniques

| Paramètre | Valeur |
|-----------|--------|
| Fréquence | 60-64 GHz (mmWave FMCW) |
| Portée | 0.1 - 30 m |
| Champ de vue | 120° (azimuth), 60° (élévation) |
| Résolution | ~5 cm (distance), ~10° (angle) |
| Cibles max | 10-12 personnes simultanées |
| Interface | UART TTL (3.3V) |
| Baudrate | 115200 |
| Alimentation | 3.3V ou 5V (selon module) |
| Montage | Plafond ou mural |

---

## Protocole de communication

### Trame binaire (datasheet §8.2)

```
┌────────┬────────┬────────┬─────────────────────┐
| HEADER | LENGTH | FRAME  | TLVs (variable)     |
| 8 octets| 4 octets| 4 octets| { type, len, payload } |
└────────┴────────┴────────┴─────────────────────┘
```

**Header :** `01 02 03 04 05 06 07 08` (fixe)  
**Length :** `uint32` little-endian (trame complète, header inclus)  
**Frame :** `uint32` little-endian (numéro de trame)

### TLV (Type-Length-Value)

| Type | Nom | Description |
|------|-----|-------------|
| 1 | Point Cloud | Nuage de points bruts (len=0 sur ce firmware) |
| 2 | Tracks | Pistes personnes (8 floats par cible) |

**Structure Track (32 octets = 8 × float32) :**

| Offset | Champ | Type | Unité |
|--------|-------|------|-------|
| 0 | Q | float | Qualité (0-100) |
| 4 | ID | float | Identifiant unique |
| 8 | X | float | Position (m) |
| 12 | Y | float | Position (m) |
| 16 | Z | float | Hauteur (m) |
| 20 | Vx | float | Vitesse X (m/s) |
| 24 | Vy | float | Vitesse Y (m/s) |
| 28 | Vz | float | Vitesse Z (m/s) |

---

## Commandes AT

### Initialisation

```python
AT_INIT = [
    b"AT+DEBUG=3\n",   # Mode 3 = trames binaires
    b"AT+START\n",     # Démarrer l'acquisition
]
```

### Autres commandes (datasheet §8.1)

| Commande | Description |
|----------|-------------|
| `AT+VERSION` | Version firmware |
| `AT+HELP` | Liste des commandes |
| `AT+CONFIG=<param>` | Configuration |
| `AT+STOP` | Arrêter l'acquisition |
| `AT+RESET` | Redémarrer le module |

---

## Test Python

### Installation dépendances

```bash
# Debian/Ubuntu
sudo apt install python3-pyqt5 python3-pyqtgraph python3-serial python3-numpy

# Ou via pip
pip install PyQt5 pyqtgraph pyserial numpy
```

### Usage

```bash
# Mode normal
python3 ms72sf1_radar_map.py

# Port série personnalisé
python3 ms72sf1_radar_map.py --port /dev/ttyUSB1

# Debug (hexdump des trames)
python3 ms72sf1_radar_map.py --debug

# Sans initialisation AT (si déjà fait)
python3 ms72sf1_radar_map.py --no-init

# Filtrage hauteur (rejette chiens, enfants assis)
python3 ms72sf1_radar_map.py --zmin 0.5 --zmax 2.5

# Désactiver filtrage zone
python3 ms72sf1_radar_map.py --no-zone-filter
```

### Options de filtrage

| Option | Description | Valeur recommandée |
|--------|-------------|-------------------|
| `--confirm N` | Trames avant validation piste | 3 (défaut) |
| `--zmin M` | Hauteur mini (m) | 0.5 (rejette chiens) |
| `--zmax M` | Hauteur maxi (m) | 2.5 (debout) |
| `--view M` | Demi-étendue vue (m) | 5.0 |

---

## Portage ESP32-S3

### Architecture cible

```
ESP32-S3 (UART) ←→ MS72SF1
    │
    ├─ ESP-NOW mesh (même proto que node-tof)
    │
    └─ MQTT (master only)
```

### Travail requis

| Tâche | Effort | Notes |
|-------|--------|-------|
| Driver UART ESP-IDF | 2h | Lecture trames binaires |
| Parser TLV | 4h | Même algo que Python |
| Filtre anti-ghost | 2h | Confirmation N trames |
| Intégration mesh ESP-NOW | 4h | Compatibilité node-tof |
| Calibration zone/Z | 2h | Config NVS |
| Payload MQTT | 2h | Format compatible backend |

**Total : ~16h**

---

## Payload MQTT (format cible)

```json
{
  "schema": "nl.telemetry.v1",
  "model":  "nl-map-out",
  "duid":   "24:EC:4A:23:36:BC",
  "ts":     1716500000,
  "metrics": {
    "targets": [
      {"id": 1, "x": 1.2, "y": -0.5, "z": 1.8, "vx": 0.3, "vy": -0.1, "vz": 0.0, "q": 85},
      {"id": 2, "x": -1.0, "y": 2.1, "z": 1.7, "vx": -0.2, "vy": 0.4, "vz": 0.0, "q": 72}
    ],
    "count": 2,
    "zone": {"xmin": -3.0, "xmax": 3.0, "ymin": -3.0, "ymax": 3.0}
  },
  "health": {
    "battery_mv": 3300,
    "rssi": -62,
    "fw": "1.0.0"
  }
}
```

**Différences vs ToF :**
- `targets[]` au lieu de `grid[][]`
- Coordonnées 3D + vitesse
- `model = "nl-map-out"`

---

## Calibration

### Montage plafond (recommandé)

```
        ┌────────────┐
        │   Radar    │  ← Plafond, hauteur H
        │    (0,0)   │
        └─────┬──────┘
              │ Z (hauteur)
              │
    ──────────┼──────────  ← Sol (Z = -H)
              │
    ◄──── X ──┼── ─────►
              │
              │ Y (profondeur)
```

**Paramètres NVS :**
- `calib_x_m` : Décalage X (m)
- `calib_y_m` : Décalage Y (m)
- `calib_z_m` : Hauteur montage (m)
- `calib_theta_deg` : Rotation (°)

### Commandes AT de calibration

```
AT+ZONE=XMIN,XMAX,YMIN,YMAX
AT+HEIGHT=H
AT+ANGLE=THETA
```

---

## Dépannage

| Symptôme | Cause probable | Solution |
|----------|----------------|----------|
| Pas de trames | Mauvais port série | `ls /dev/ttyUSB*` |
| Trames corrompues | Baudrate incorrect | Vérifier 115200 |
| Cibles fantômes | Sensibilité trop haute | `AT+SENS=N` (réduire) |
| Perte de cibles | Zone trop petite | `AT+ZONE=...` (élargir) |
| Z incorrect | Hauteur montage erronée | `AT+HEIGHT=H` |

---

## Références

- Datasheet MS72SF1 : `docs/MS72SF1_Datasheet.pdf`
- Protocole §8.2 : `docs/MS72SF1_Protocol.pdf`
- Commandes AT §8.1 : `docs/MS72SF1_AT_Commands.pdf`

---

*Créé : 24 Mai 2026*  
*Statut : Test Python OK, portage ESP32 à faire*
