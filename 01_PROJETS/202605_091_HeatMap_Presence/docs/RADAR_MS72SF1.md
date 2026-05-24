# Radar mmWave MS72SF1 — Intégration NOISYLESS

## Contexte

Le MS72SF1 est un radar mmWave 60-64 GHz de MinewSemi, utilisé pour la détection de présence **extérieur** (insensible lumière du jour).

**Problème résolu :** Le ToF TMF8829 ne fonctionne pas en extérieur diurne (portée ~0.6m en plein soleil). Le radar prend le relais pour les zones extérieures.

---

## État actuel

### ✅ Test Python validé

**Fichier :** `firmware/node-radar-ms72sf1/ms72sf1_radar_map.py`

**Fonctionnalités :**
- Lecture série UART (115200 baud)
- Parsing trames binaires TLV (Header + Length + Frame + TLVs)
- Extraction cibles : `id, x, y, z, vx, vy, vz, q`
- Affichage temps réel PyQtGraph :
  - Position + ID + trajectoire
  - Vecteurs vitesse
  - Comptage global
- Filtrage anti-faux-positifs :
  - Confirmation N trames (anti-ghost)
  - Filtre hauteur Z (rejette chiens)
  - Filtre zone X/Y

**Dépendances :**
```bash
pip install PyQt5 pyqtgraph pyserial numpy
```

**Usage :**
```bash
python3 ms72sf1_radar_map.py --port /dev/ttyUSB0 --zmin 0.5 --zmax 2.5
```

---

### 🟡 Portage ESP32-S3 — À faire

**Architecture cible :**

```
┌─────────────────┐
│  ESP32-S3       │
│  (UART + WiFi)  │
│                 │
│  ┌───────────┐  │
│  │ UART RX   │◄─┼── MS72SF1 (115200 baud)
│  │ Parser    │  │
│  │ TLV       │  │
│  └─────┬─────┘  │
│        │        │
│  ┌─────▼─────┐  │
│  │ ESP-NOW   │  │──→ Master node (même proto ToF)
│  │ mesh      │  │
│  └───────────┘  │
└─────────────────┘
```

**Travail requis (~16h) :**

| Tâche | Effort | Détails |
|-------|--------|---------|
| Driver UART ESP-IDF | 2h | Lecture non-blocante, buffer circulaire |
| Parser TLV binaire | 4h | Même algo que Python (struct.unpack) |
| Filtre anti-ghost | 2h | Confirmation N trames, purge stale |
| Intégration ESP-NOW | 4h | Payload compatible node-tof |
| Calibration NVS | 2h | Zone, hauteur, rotation |
| Payload MQTT master | 2h | Format `nl-map-out` |

**Livrables :**
- `components/ms72sf1/` — Driver capteur
- `components/radar_parser/` — Parsing TLV
- `main_radar.c` — App principale (similaire à node-tof)
- `platformio.ini` — Build config

---

## Comparaison ToF vs Radar

| Critère | TMF8829 (ToF) | MS72SF1 (Radar) |
|---------|---------------|-----------------|
| **Technologie** | dToF (lumière) | FMCW (ondes radio) |
| **Fréquence** | 940 nm (IR) | 60-64 GHz |
| **Portée** | ~5 m intérieur | ~30 m intérieur/extérieur |
| **Lumière jour** | ❌ Échec (0.6 m) | ✅ Insensible |
| **Précision** | ~5 cm | ~5-10 cm |
| **Champ** | 60-90° | 120° azimuth |
| **Cibles** | Grille 8×8 ou 16×16 | 10-12 pistes 3D |
| **Sortie** | Distances par zone | X/Y/Z + Vx/Vy/Vz |
| **Conso** | ~50 mA (scan) | ~200-400 mA |
| **Prix** | ~15-20 € | ~40-60 € |

**Conclusion :**
- **Intérieur :** ToF (moins cher, conso réduite, précision suffisante)
- **Extérieur :** Radar (seule option diurne)

---

## Intégration backend

### Payload MQTT (format unifié)

```json
{
  "schema": "nl.telemetry.v1",
  "model":  "nl-map-out",
  "duid":   "24:EC:4A:23:36:BC",
  "ts":     1716500000,
  "metrics": {
    "type": "targets_3d",
    "count": 2,
    "targets": [
      {"id": 1, "x": 1.2, "y": -0.5, "z": 1.8, "vx": 0.3, "vy": -0.1, "q": 85},
      {"id": 2, "x": -1.0, "y": 2.1, "z": 1.7, "vx": -0.2, "vy": 0.4, "q": 72}
    ]
  },
  "health": {"battery_mv": 3300, "rssi": -62, "fw": "1.0.0"}
}
```

### Modifications backend requises

| Service | Changement | Effort |
|---------|------------|--------|
| `mqtt-ingest` | Route par `model = "nl-map-out"` | 1h |
| `heatmap-agg` | Agrégation cibles 3D (pas grille) | 4h |
| `api` | Endpoint `GET /api/villas/{id}/radar` | 2h |
| Dashboard | Visualisation 3D (Three.js ?) | 8h |

**Total backend : ~15h**

---

## Calibration & Montage

### Hauteur de montage

| Usage | Hauteur recommandée |
|-------|---------------------|
| Petit bureau | 2.5 - 3.0 m |
| Salon | 3.0 - 4.0 m |
| Terrasse | 3.5 - 5.0 m |
| Parking | 4.0 - 6.0 m |

### Orientation

```
        Nord (θ=0°)
           ↑
           │
    Ouest ←┼→ Est
    (90°)  │ (270°)
           │
        Sud (180°)
```

**Paramètre NVS :** `calib_theta_deg` (rotation horaire depuis Nord)

### Zone de détection

**Commandes AT (à tester) :**
```
AT+ZONE=-3.0,3.0,-3.0,3.0   # Xmin, Xmax, Ymin, Ymax (m)
AT+HEIGHT=3.5               # Hauteur montage (m)
AT+ANGLE=180                # Rotation (°)
AT+SENS=5                   # Sensibilité (1-10)
```

---

## Dépannage

### Problèmes courants

| Symptôme | Cause | Solution |
|----------|-------|----------|
| Pas de trames | UART mal branché | Vérifier TX/RX croisés |
| Trames corrompues | Baudrate incorrect | 115200 uniquement |
| Cibles fantômes | Sensibilité haute | `AT+SENS=3` (réduire) |
| Perte cibles | Zone trop petite | Élargir `AT+ZONE` |
| Z incorrect | Hauteur erronée | `AT+HEIGHT=H` |
| ID qui changent | Radar redémarré | Normal (ID non persistants) |

### Debug

```bash
# Voir trames brutes
python3 ms72sf1_radar_map.py --debug

# Sans initialisation AT (déjà fait)
python3 ms72sf1_radar_map.py --no-init

# Logging série (screen)
screen /dev/ttyUSB0 115200
```

---

## Roadmap

### Semaine 1 — Test Python
- [x] Script Python validé
- [ ] Test sur matériel réel (à recevoir)
- [ ] Calibration zone/hauteur
- [ ] Documentation commandes AT

### Semaine 2 — Firmware ESP32
- [ ] Driver UART ESP-IDF
- [ ] Parser TLV
- [ ] Intégration mesh ESP-NOW
- [ ] Payload MQTT compatible

### Semaine 3 — Backend
- [ ] Route par `model = "nl-map-out"`
- [ ] Agrégation cibles 3D
- [ ] Endpoint API radar
- [ ] Dashboard 3D (optionnel)

---

## Références

- **Datasheet :** `../../docs/MS72SF1_Datasheet.pdf`
- **Protocole :** `../../docs/MS72SF1_Protocol_v8.2.pdf`
- **Commandes AT :** `../../docs/MS72SF1_AT_Commands.pdf`
- **Test Python :** `../firmware/node-radar-ms72sf1/ms72sf1_radar_map.py`

---

*Créé : 24 Mai 2026*  
*Statut : Test Python OK, hardware en attente*
