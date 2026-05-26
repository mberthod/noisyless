# 202604_Projet_089_NOISYLESS_PROJET_PRINCIPAL

## Contexte
- **Client** : NOISYLESS (produit interne MBHREP — filiale MBHHOLD)
- **Entité** : MBHREP/ELEC-CORE
- **Démarrage** : 2026-04
- **Phase actuelle** : MVP V1 en finalisation (Rev B)
- **Dernière activité** : 2026-04-27
- **Localisation Kdrive** : `~/Kdrive/01_CLIENTS/NOISYLESS/202604_089_Capteur_Environnemental_V1/`
- **Site produit** : noisyless.com (OVH FTP)
- **Agent commercial** : ARTEMIS (profil Hermes MBHOLD)

## Description
Système IoT de capteurs discrets mesurant le **bruit** et la **qualité d'air** en temps réel dans les établissements d'hébergement (hôtels, Airbnb, résidences). Dashboard centralisé + alertes seuil.

**Positionnement vs Minut** : NOISYLESS = écosystème du confort (Air + Son + Expérience), pas juste un détecteur. Valeur : transformer la donnée environnementale en avantage commercial.

## Stack technique

### Hardware — Environmental Sensor V1 (Rev B)
| Composant | Rôle | Détail |
|-----------|------|--------|
| MCU | ESP32-S3-MINI-1 | WiFi 2.4 GHz + BLE 5, dual-core Xtensa LX7 |
| Qualité d'air | BME680 (I2C) + BSEC2 | IAQ, CO₂eq, bVOCeq, T°C, humidité, pression |
| Présence | LD2410 | Radar mmWave, UART 256000 bauds, détection mouvement + stationnaire |
| Luminosité | 2× LDR A1050 (analogique) | Mesure lumière ambiante |
| Temp/Humidité (redondant) | HDC1080 (I2C) | Backup T°C + humidité |
| Audio | 2× micros électret POM-2244P-C3310 | Mesure pic-à-pic 50 ms |
| Alimentation | USB-C + régulateur LDO AP7362-33SP | 3.3V, 1.5A |
| Connectique | RJ9 4P4C + USB-C + switch ON/OFF + BOOT | — |
| LEDs | RGB (Rouge, Vert, Bleu) actif bas | — |
| Flash | 8 MB | Partitionnement perso : SPIFFS + OTA |
| Boîtier | Impression 3D | Fond, couvercle, support — fichiers .3mf |
| Alim secteur | USB-C | Pas de batterie pour ce SKU bruit |

### Firmware — PlatformIO
| Élément | Stack |
|---------|-------|
| Framework | Arduino + FreeRTOS |
| Connectivité | WiFi + MQTT TLS (PubSubClient, certificat CA racine) |
| OTA | Manifest GitHub (stable.json), vérification SHA-256 |
| Librairies | BSEC2 (Bosch), LD2410 (ncmreynolds), PubSubClient, ArduinoJson, WiFiManager |
| Tâches FreeRTOS | Memory Task, Sensor Task, Network Task, Button Task |
| Publication | JSON unique toutes les 10s vers broker MQTT (Telegraf-compatible) |
| Device ID | NL-001, FW v1.0.0 |
| Offline | Stockage local + reprise auto quand WiFi restauré |
| Repo Git | `mberthod/noisyless` |

### Infrastructure Backend
| Élément | Configuration |
|---------|---------------|
| Serveur | VPS Hetzner CAX11 — 91.99.26.43 |
| SSH | `noisyless@91.99.26.43` |
| Code | `/opt/noisyless/`, docker-compose.yml existant |
| Broker | Mosquitto MQTT (TLS) |
| Stack | Python/FastAPI, SvelteKit, TimescaleDB |
| Dashboard | SvelteKit sur noisyless.com |
| RGPD | Rétention max 90 jours, données prétraitées côté node, docs `docs/rgpd/` |

## Roadmap
| Phase | Objectif | Statut |
|-------|----------|--------|
| **Phase 1** | Finaliser MVP V1, validation commerciale | 🟡 En cours |
| **Phase 2** (6 mois) | Fiabilité, intégrations tierces | ⚪ Planification |
| **Phase 3** (Futur) | Extension géographique | ⚪ Long terme |

## Pricing
| Offre | Prix |
|-------|------|
| Starter | 4,99 €/mois |
| HeatMap upsell | +10-20 €/mois/logement |
| Modèle | Abonnement mensuel récurrent, pas d'essai gratuit |

## Pipeline commercial (PostgreSQL AOS-Core)
| Client | Potentiel | Statut |
|--------|-----------|--------|
| Ibis Managed Services | 15 000 € | Actif |
| Adagio Access | 18 000 € | Actif |
| Starhotels | 15 000 € | Actif |
| Elior Residences | 12 000 € | Draft accepté ✅ |
| La Cite Urbaine | 10 000 € | Actif |
| GuestReady, Welkeys, Ouikey, Keylodge, HostnFly | À qualifier | Prospect |

## Risques / sujets ouverts
- 5 drafts emails en attente validation T3 (fichier `~/noisyless/drafts-a-valider/5-hotels.md`)
- Documentation technique à compléter côté MBHREP
- Pas de dossier Kdrive sous `01_CLIENTS/NOISYLESS/` pour le projet principal — schémas probablement dans `01_ELECTRONIQUE/` ou sur Git

## Prochaines actions
- [ ] Indexer les schémas KiCad et BOM du capteur V1 dans la base MBHREP
- [ ] Valider et envoyer les 5 drafts hôtels (priorité Elior Residences)
- [ ] Clarifier où sont les sources hardware du projet principal (Git? Kdrive?)
