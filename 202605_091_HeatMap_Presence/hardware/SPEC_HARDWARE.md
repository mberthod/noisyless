# Hardware Specification — Node ToF Intérieur (#091)

> **Version** : 1.0-draft | **Date** : 2026-05-23 | **Cible** : PCB 50×50mm max, 2 couches
> **Base** : Projet LVMH #062 (MCU.kicad_sch) → adapter pour batterie + TMF8829

---

## 1. Schéma bloc

```
┌─────────────────────────────────────────────────────┐
│                   BATTERY SECTION                    │
│                                                      │
│  18650 Li-Ion ──┬── TP4056 Charge Controller ── USB-C│
│  (3.7V nom)     │   + DW01 Protection IC             │
│                 │                                     │
│          ┌──────┴──────┐                              │
│          │  LDO 3.3V   │──→ VDD_3V3 (ESP + sensors)  │
│          │  AP7362-33SP │                              │
│          │  1.5A max    │                              │
│          └─────────────┘                              │
└─────────────────────────────────────────────────────┘
                         │
┌────────────────────────┼────────────────────────────┐
│                   MCU SECTION                         │
│                   ESP32-S3-MINI-1                     │
│                                                      │
│  GPIO8  ─── I²C SDA ─── TMF8829                     │
│  GPIO9  ─── I²C SCL ─── TMF8829                     │
│  GPIO4  ─── TMF8829 INT (optional)                   │
│  GPIO1  ─── ADC1_CH0 ── Battery voltage divider      │
│  GPIO38 ─── LED Blue                                 │
│  GPIO47 ─── LED Green                                │
│  GPIO48 ─── LED Red                                  │
│  GPIO0  ─── BOOT button (pull-up, GND to flash)      │
│  EN     ─── Reset + RC delay                         │
│  GPIO3  ─── UART0 TX (debug)                         │
│  GPIO2  ─── UART0 RX (debug)                         │
│                                                      │
│  Antenna: PCB trace or U.FL external                 │
└─────────────────────────────────────────────────────┘
                         │
┌────────────────────────┼────────────────────────────┐
│                 SENSOR SECTION                        │
│                   TMF8829 dToF                         │
│                                                      │
│  I²C addr 0x29                                       │
│  VDD = 3.3V                                          │
│  VCSEL = 3.3V (shared with VDD for simplicity)       │
│  GPIO0 = INT (optional, not used in V1)              │
│  GPIO1 = not connected                               │
│  Decoupling: 100nF + 10µF on VDD, 100nF on VCSEL    │
└─────────────────────────────────────────────────────┘
```

---

## 2. BOM (Bill of Materials)

| Ref | Qty | Value | Package | Description |
|-----|-----|-------|---------|-------------|
| **MCU & Wireless** | | | | |
| U1 | 1 | ESP32-S3-MINI-1 | SMD module | Espressif, 8MB flash, PCB antenna |
| **ToF Sensor** | | | | |
| U2 | 1 | TMF8829 | OLGA-26 | ams-OSRAM dToF, I²C |
| **Power** | | | | |
| U3 | 1 | AP7362-33SP | SOP-8 | 3.3V LDO, 1.5A |
| U4 | 1 | TP4056 | SOP-8 | Li-Ion charger, USB-C input |
| U5 | 1 | DW01 + FS8205 | SOT-23-6 + TSSOP-8 | Battery protection IC |
| **Connectors** | | | | |
| J1 | 1 | USB-C 16-pin | USB-C receptacle | Power only (no data) |
| J2 | 1 | 18650 holder | THT | Battery holder (or solder tabs) |
| J3 | 1 | U.FL | SMD | Optional external antenna |
| J4 | 1 | 3-pin header | 2.54mm | UART debug (TX/RX/GND) |
| **LED** | | | | |
| D1 | 1 | LED RGB Common Anode | 5050 PLCC-4 | Status indicator |
| R1,R2,R3 | 3 | 220Ω | 0603 | LED current limit |
| **Passives** | | | | |
| C1,C2 | 2 | 10µF | 0805 | VDD decoupling |
| C3,C4,C5 | 3 | 100nF | 0603 | VDD decoupling |
| C6 | 1 | 100nF | 0603 | VCSEL decoupling |
| C7 | 1 | 22µF | 0805 | Battery bulk cap |
| R4 | 1 | 100kΩ | 0603 | Voltage divider R1 |
| R5 | 1 | 100kΩ | 0603 | Voltage divider R2 (VBAT/2) |
| R6,R7 | 2 | 10kΩ | 0603 | I²C pull-ups |

---

## 3. Instructions — Modification du projet LVMH

### 3.1 Ouvrir le projet LVMH dans KiCad
```
kicad ~/Kdrive/01_CLIENTS/LVMH/202510_Projet_062_LVMH_HeatMap/03_CAO/01_Schemas/202510_Projet_062_LVMH_HeatMap/202510_Projet_062_LVMH_HeatMap.kicad_pro
```

### 3.2 Sauvegarder comme nouveau projet
- File → Save As → `~/Kdrive/01_CLIENTS/NOISYLESS/202605_091_HeatMap_Presence/hardware/kicad/heatmap-tof.kicad_pro`

### 3.3 Feuilles à supprimer
- `ETHERNET.kicad_sch` — plus de PoE/W5500
- `RADAR.kicad_sch` — le radar est un node séparé

### 3.4 Feuille MCU — modifications
- **Garder** : ESP32-S3-MINI-1, découplage, boot, reset, UART debug, LED RGB
- **Supprimer** : connecteur RJ45, W5500, circuit PoE (TPS2375)
- **Remplacer** : alim secteurs → alim batterie 18650

### 3.5 Nouvelle feuille — alim batterie
- TP4056 + USB-C pour charger
- Protection DW01 + FS8205
- AP7362-33SP LDO 3.3V
- Diviseur résistif sur ADC1_CH0 (GPIO1)

### 3.6 Nouvelle feuille — TMF8829
- I²C sur GPIO8/GPIO9 (partagés avec le bus existant)
- Alim 3.3V avec découplage
- Pull-ups 10kΩ sur SDA/SCL

### 3.7 Layout
- Format max 50×50mm
- Antenne ESP32 en bord de carte, zone dégagée (pas de cuivre)
- TMF8829 centré, optique vers le bas (bottom layer si possible, ou cutout)
- USB-C sur un bord pour accès boîtier
- UART debug accessible via pastilles ou header 3-pin

---

## 4. Notes de design

### Antenne ESP32
- PCB trace antenna intégrée au module MINI-1
- Zone keep-out : pas de cuivre sous l'antenne ni à <5mm autour, tous les layers
- Option U.FL pour antenne externe si boîtier métallique

### TMF8829 layout
- Optique dégagée : ouverture dans le boîtier + fenêtre
- Pas de composants hauts <2mm autour du capteur
- Zone de garde : pas de vias sous le capteur

### Batterie
- TP4056 charge à 500mA par défaut (PROG resistor 2.4kΩ)
- DW01 coupe à 2.4V (décharge profonde) et 4.28V (survoltage)
- LDO AP7362 dropout ~400mV à 1A → OK avec 3.7V Li-Ion vers 3.3V

### Connecteurs
- USB-C : VBUS, GND, CC1, CC2 uniquement (pas de data)
- CC1/CC2 : pull-down 5.1kΩ pour négociation 5V
- 18650 : prévoir soudure sur pastilles ou holder clipsable
