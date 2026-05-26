# NOISYLESS Simple — `nl-presence`

**Statut :** En développement  
**Cible :** Détection présence basique + qualité d'air  
**Prix :** ~50 € BOM

---

## 📋 Spécifications

### Capteurs

| Capteur | Rôle | Interface |
|---------|------|-----------|
| PIR HC-SR501 | Présence binaire | GPIO (IRQ) |
| VL53L0X | Distance mono-zone | I2C |
| SCD4x | CO₂, T°, HR | I2C |
| INMP441 | Niveau sonore (dB) | I2S |

### MCU

| Paramètre | Valeur |
|-----------|--------|
| Modèle | ESP32-S3-MINI-1 |
| Flash | 8 MB |
| WiFi | 2.4 GHz |
| BLE | 5.0 |
| Antenne | PCB ou U.FL |

### Alimentation

| Mode | Conso | Autonomie |
|------|-------|-----------|
| Actif | ~80 mA | — |
| Deep sleep | ~50 µA | ~2 ans (18650) |
| Secteur | USB-C 5V | Permanent |

---

## 📁 Structure du firmware

```
firmware/node-presence/
├── src/
│   ├── main.c              # App principale
│   ├── sensors/
│   │   ├── pir.c           # Driver PIR
│   │   ├── tof.c           # Driver VL53L0X
│   │   ├── scd4x.c         # Driver CO₂
│   │   └── mic.c           # Driver INMP441
│   ├── mesh/
│   │   ├── espnow.c        # ESP-NOW mesh
│   │   └── mqtt.c          # MQTT client
│   └── config/
│       ├── nvs.c           # Config NVS
│       └── ota.c           # OTA updates
├── platformio.ini
└── README.md
```

---

## 🔧 Installation firmware

### Prérequis

```bash
# Installer PlatformIO
pip install platformio

# Ou VS Code + extension PlatformIO
```

### Build

```bash
cd firmware/node-presence
pio run
```

### Flash USB

```bash
# Mode bootloader (tenir BOOT + RESET)
pio run --target upload
```

### Flash OTA (après premier claim)

```bash
# Le device récupère sa config WiFi + MQTT via provisioning
# Puis les mises à jour sont automatiques
```

---

## 📡 Payload MQTT

### Topic
```
noisyless/<duid>/telemetry
```

### Format
```json
{
  "schema": "nl.telemetry.v1",
  "model": "nl-presence",
  "duid": "24:EC:4A:23:36:BC",
  "ts": 1716500000,
  "metrics": {
    "present": true,
    "co2_ppm": 450,
    "temp_c": 22.5,
    "hum_pct": 45,
    "sound_db": 35
  },
  "health": {
    "battery_mv": 3300,
    "rssi": -62,
    "fw": "1.0.0"
  }
}
```

### Fréquence
- **Secteur :** Toutes les 30s
- **Batterie :** Toutes les 5min + delta significatif

---

## 🔐 Provisioning

### Méthode 1 : QR Code (recommandé)

1. Scanner QR sur étiquette device
2. App → WiFi credentials → device
3. Device → phone-home → backend
4. Backend → claim automatique

### Méthode 2 : SoftAP

1. Device boot → AP `NOISY-XXXX`
2. Connect → http://192.168.4.1
3. Config WiFi + MQTT
4. Redémarrage

### Méthode 3 : Claim code (secours)

1. Code HMAC sur étiquette
2. API POST `/devices/claim`
3. Validation manuelle

---

## 🏠 Installation physique

### Montage

| Emplacement | Hauteur | Fixation |
|-------------|---------|----------|
| Plafond | 2.5-3.5 m | Vis ou adhésif |
| Mur | 1.5-2.0 m | Vis |
| Bureau | — | Posé |

### Orientation

```
        ┌─────────┐
        │  PIR    │  ← Face à la zone à surveiller
        │  ● ●    │
        └─────────┘
           │
        Capteur
           │
        ▼ Zone de détection (120°)
```

### Portée PIR

- **Détection :** ~7 m
- **Angle :** 120°
- **Hauteur max :** 3.5 m

---

## 🆘 Dépannage

| Symptôme | Cause | Solution |
|----------|-------|----------|
| LED ne s'allume pas | Pas d'alim | Vérifier USB-C |
| Pas de WiFi | Mauvais credentials | Refaire provisioning |
| CO₂ = 400 constant | SCD4x pas init | Redémarrer device |
| PIR toujours actif | Face à source chaleur | Déplacer capteur |
| Battery drain | Deep sleep bug | Flash firmware v1.1+ |

---

## 📊 Backend integration

### Tables DB utilisées

```sql
-- Devices
SELECT * FROM noisyless.devices WHERE model = 'nl-presence';

-- Mesures
SELECT time, payload->>'co2_ppm' AS co2
FROM noisyless.measurements
WHERE model = 'nl-presence'
ORDER BY time DESC
LIMIT 100;

-- Agrégats 5min
SELECT window_start, avg((payload->>'co2_ppm')::int) AS avg_co2
FROM noisyless.measurements_agg_5min
WHERE model = 'nl-presence'
GROUP BY window_start;
```

### Alertes configurables

```sql
-- Inscrire une alerte CO₂
INSERT INTO noisyless.alert_thresholds (villa_id, param, threshold)
VALUES ('villa_001', 'co2_ppm', 1000);

-- Inscrire une alerte présence (fête)
INSERT INTO noisyless.alert_thresholds (villa_id, param, threshold)
VALUES ('villa_001', 'present_count', 10);
```

---

## 📄 Fichiers du projet

| Fichier | Rôle |
|---------|------|
| `firmware/node-presence/` | Code ESP32 |
| `hardware/schematics/` | Schémas électroniques |
| `hardware/bom/` | Liste composants |
| `docs/QUICKSTART.md` | Démarrage rapide |
| `docs/INSTALL.md` | Guide installation |
| `scripts/flash.sh` | Script flash |

---

## 🔗 Liens

- **Dashboard :** https://platform.noisyless.com
- **API docs :** https://api.platform.noisyless.com/docs
- **Infra deployment :** `../../_infra/docs/DEPLOYMENT.md`

---

*Créé : 24 Mai 2026*  
**Statut :** Firmware 50%, Hardware 0%, Backend 80%
