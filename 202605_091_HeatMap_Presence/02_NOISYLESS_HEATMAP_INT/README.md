# NOISYLESS HeatMap Intérieur — `nl-map-in`

**Statut :** Pilote fonctionnel  
**Cible :** Heatmap intérieure (ToF 16×16)  
**Prix :** ~80 € BOM

---

## 📋 Spécifications

### Capteurs

| Capteur | Rôle | Interface |
|---------|------|-----------|
| TMF8829 | ToF 16×16 zones | I2C |
| ESP32-S3 | MCU + WiFi | — |

### MCU

| Paramètre | Valeur |
|-----------|--------|
| Modèle | ESP32-S3-MINI-1 |
| Flash | 8 MB |
| WiFi | 2.4 GHz |
| BLE | 5.0 |

### Alimentation

| Mode | Conso |
|------|-------|
| Actif | ~150 mA |
| Deep sleep | ~100 µA |
| Secteur | USB-C 5V |

---

## 📁 Structure du firmware

```
firmware/node-tof-esp32s3/
├── src/
│   ├── main.c              # App principale
│   ├── components/
│   │   ├── tmf8829/        # Driver ToF
│   │   ├── cluster/        # Clustering 4-way
│   │   ├── mesh/           # ESP-NOW mesh
│   │   └── config/         # NVS config
│   └── power.c             # Gestion énergie
├── platformio.ini
└── README.md
```

---

## 🔧 Installation firmware

### Build

```bash
cd firmware/node-tof-esp32s3
pio run
```

### Flash

```bash
pio run --target upload
```

---

## 📡 Payload MQTT

```json
{
  "schema": "nl.telemetry.v1",
  "model": "nl-map-in",
  "duid": "24:EC:4A:23:36:BC",
  "ts": 1716500000,
  "metrics": {
    "grid": [[0,1,2],[1,3,1],[0,1,0]],
    "total_count": 5,
    "max_count": 3
  },
  "health": {
    "battery_mv": 3300,
    "rssi": -62,
    "fw": "1.0.0"
  }
}
```

---

## 🏠 Installation

### Montage plafond (recommandé)

```
        ┌─────────────┐
        │   ToF       │  ← Plafond, 2.5-3.5 m
        │  (16×16)    │
        └──────┬──────┘
               │
        ▼ Zone 5×5 m
```

### Multi-noeuds (mesh)

```
  ┌─────┐    ┌─────┐    ┌─────┐
  │Node1│◄──►│Node2│◄──►│Node3│  ← ESP-NOW mesh
  └──┬──┘    └──┬──┘    └──┬──┘
     │          │          │
     └──────────┴──────────┘
                │
           ┌────▼────┐
           │ Master  │  → MQTT
           └─────────┘
```

---

## 📊 Backend

### Tables utilisées

```sql
-- Heatmap agrégée 5min
SELECT window_start, villa_id, zone, total_count, max_count
FROM noisyless.heatmap_agg_5min
WHERE villa_id = 'villa_001'
ORDER BY window_start DESC
LIMIT 12;
```

### Alertes

```sql
-- Suroccupation
INSERT INTO noisyless.alert_thresholds (villa_id, param, threshold)
VALUES ('villa_001', 'max_count', 10);
```

---

## 🆘 Dépannage

| Symptôme | Cause | Solution |
|----------|-------|----------|
| Pas de mesures | ToF pas init | Vérifier I2C (SDA/SCL) |
| Grid vide | Personne hors portée | Portée max ~5 m |
| Mesh instable | Nodes trop loin | Max 30 m entre nodes |

---

*Créé : 24 Mai 2026*  
**Statut :** Firmware 90%, Backend 80%
