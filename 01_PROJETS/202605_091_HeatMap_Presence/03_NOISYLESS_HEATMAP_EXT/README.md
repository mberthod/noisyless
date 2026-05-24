# NOISYLESS HeatMap Extérieur — `nl-map-out`

**Statut :** En développement  
**Cible :** Heatmap extérieure (Radar 60 GHz)  
**Prix :** ~120 € BOM

---

## 📋 Spécifications

### Capteurs

| Capteur | Rôle | Interface |
|---------|------|-----------|
| MS72SF1 | Radar 60 GHz | UART |
| ESP32-S3 | MCU + WiFi | — |

### Performances

| Paramètre | Valeur |
|-----------|--------|
| Portée | 0.5 - 30 m |
| Champ | 120° azimuth |
| Cibles max | 10-12 |
| Insensible | Lumière, pluie, vent |

---

## 📁 Structure du firmware

```
firmware/node-radar-ms72sf1/
├── src/
│   ├── main.c              # App principale
│   ├── components/
│   │   ├── ms72sf1/        # Driver UART
│   │   ├── parser/         # Parsing TLV
│   │   └── mesh/           # ESP-NOW
│   └── config/
├── platformio.ini
└── README.md
```

---

## 🔧 Test Python (validé)

```bash
# Installer dépendances
pip install PyQt5 pyqtgraph pyserial numpy

# Lancer l'app
python3 ms72sf1_radar_map.py --port /dev/ttyUSB0 --zmin 0.5 --zmax 2.5
```

---

## 📡 Payload MQTT

```json
{
  "schema": "nl.telemetry.v1",
  "model": "nl-map-out",
  "duid": "24:EC:4A:23:36:BC",
  "ts": 1716500000,
  "metrics": {
    "targets": [
      {"id": 1, "x": 1.2, "y": -0.5, "z": 1.8, "vx": 0.3, "q": 85}
    ],
    "count": 1
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

### Montage extérieur

```
        ┌─────────────┐
        │   Radar     │  ← IP65, -20 à +60°C
        │  60 GHz     │
        └──────┬──────┘
               │
        ▼ Zone 30×30 m
```

### Alimentation

- **Secteur :** USB-C 5V
- **PoE :** Option avec injecteur
- **Batterie :** 18650 + panneau solaire (option)

---

## 🆘 Dépannage

| Symptôme | Cause | Solution |
|----------|-------|----------|
| Pas de trames | UART mal branché | Croiser TX/RX |
| Cibles fantômes | Sensibilité haute | `AT+SENS=3` |
| Z incorrect | Hauteur montage | `AT+HEIGHT=H` |

---

*Créé : 24 Mai 2026*  
**Statut :** Test Python OK, Firmware ESP32 30%
