# Comment flasher un capteur NOISYLESS

**TL;DR** :
```bash
cd /home/mathieu/noisyless/05_FIRMWARE
source /home/mathieu/.pio-venv/bin/activate
pio run -e esp32dev -t upload --upload-port /dev/ttyACM0
```

---

## Prerequis

- ESP32-S3 connecte en USB
- User dans groupe `dialout` (`id` doit montrer `dialout`)
- PlatformIO installe (venv `/home/mathieu/.pio-venv/`)

## Commandes

### Compilation seule
```bash
cd /home/mathieu/noisyless/05_FIRMWARE
source /home/mathieu/.pio-venv/bin/activate
pio run -e esp32dev
```
Output : `.pio/build/esp32dev/firmware.bin` (~970 KB)

### Compile + flash
```bash
pio run -e esp32dev -t upload --upload-port /dev/ttyACM0
```

### Lecture des logs serie
```bash
pio device monitor -p /dev/ttyACM0 -b 115200
```

Pour quitter le monitor : `Ctrl+C` puis `Ctrl+]`.

### Reset programme (sans debrancher)
```python
import serial, time
s = serial.Serial('/dev/ttyACM0', 115200)
s.setDTR(False); s.setRTS(True); time.sleep(0.2); s.setRTS(False)
```

## Configuration avant flash

### WiFi (actuellement hardcode)
Editer `05_FIRMWARE/src/main.cpp` lignes ~36-37 :
```cpp
static const char* WIFI_SSID = "TP-Link_B150";
static const char* WIFI_PASS = "12902637";
```

### MQTT broker
`05_FIRMWARE/include/credentials.h` :
```cpp
static const char* MQTT_HOST = "91.99.26.43";   // VPS
static const uint16_t MQTT_PORT = 1883;
static const char* MQTT_USER = "esp32";
static const char* MQTT_PASS = "<voir Bitwarden>";
```

### Device ID (actuellement hardcode)
`05_FIRMWARE/src/main.cpp` ligne ~41 :
```cpp
static const char* DEVICE_ID = "NL-001";
```

ATTENTION : si tu flashes plusieurs capteurs sans changer cette ligne, ils auront tous le meme ID -> conflit.

## Verification post-flash

1. **Logs serie** : doit afficher `[WiFi] Connecte`, `[MQTT] Connecte`, `[PUB] Publish OK ? YES` toutes les 10s
2. **Topic MQTT** : `esp32/noisyless/<MAC_HEX_MAJ>/datas`
3. **Donnees en DB** :
   ```bash
   ssh noisyless@91.99.26.43 "docker exec noisyless-timescaledb-1 psql -U noisyless -d noisyless -c \\\"SELECT time, device_id, temp, presence, distance_cm FROM measurements ORDER BY time DESC LIMIT 3\\\""
   ```
4. **Dashboard** : `https://platform.noisyless.com/dashboard.html` (apres login magic-link)

## Erreurs courantes

### "Permission denied: '/dev/ttyACM0'"
User pas dans groupe `dialout`. Faire :
```bash
sudo usermod -aG dialout $USER
# Puis logout/login
```

### "Tool Manager: Installing... 0%" et bloque
Premiere installation. Telecharge ~200 MB de toolchain ESP32-S3 + framework Arduino. Patience (~10 min).

### "[LD2410] Pas de reponse"
Voir `ARCHITECTURE.md` section 3.2 : capteur LD2410 defectueux ou cable. Verifier:
- LED rouge sur le module LD2410 (allumee = capteur OK)
- Alimentation 5V (pas 3.3V)
- TX du LD2410 -> GPIO 18 ESP32
- RX du LD2410 -> GPIO 17 ESP32
- GND commun

### Build echoue avec "fatal error: ld2410.h: No such file"
La lib n'est pas installee. Faire :
```bash
pio pkg install -e esp32dev
```

### Connexion WiFi qui boucle
Verifier le SSID/pass hardcode dans `main.cpp`. Si tu changes de WiFi, il faut recompiler + reflasher.
