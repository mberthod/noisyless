# 202604_Projet_091_NOISYLESS_HeatMap

## Contexte
- **Client** : NOISYLESS (produit interne — P1 North Star 200k€)
- **Entité** : MBHREP/ELEC-CORE
- **Démarrage** : 2026-05 (conception)
- **Phase actuelle** : `conception` — étude préliminaire
- **Localisation Kdrive** : `~/Kdrive/01_CLIENTS/NOISYLESS/202605_091_HeatMap_Presence/`
- **Budget estimé** : 6 800 € HT
- **North Star** : Contribue aux +200 000 € perso si le pilote convertit

## Description
**Heatmap de présence sans caméra** — extension premium NOISYLESS pour villas/locations haut de gamme.
Usage : détection de suroccupation/fête via comptage de personnes intérieur + extérieur → heatmap de densité + alertes seuil.
**Pas du comptage métrologique** — détection de patterns, pas des chiffres exacts (±1).
Revenu : **SaaS récurrent par villa** (pas de marge hardware).

## Règle d'or technique
> **Le ToF ne marche pas en extérieur diurne** (plein soleil → portée ~0,6 m).
> → Architecture **hétérogène obligatoire** : ToF intérieur, Radar 60 GHz extérieur.

## Architecture

```
VILLA
├── INTÉRIEUR : Nodes ToF TMF8829 ×8-15 (batterie)
│   └── Montage plafond zénithal, FOV 67,9°×52,8°
├── EXTÉRIEUR : Nodes Radar LD6001A ×3-7 (solaire + batterie 3j autonomie)
│   └── Terrasse ×2-4, Jardin ×1-3
│
├── WiFi Mesh (ESP-NOW) — pas de gateway séparée
│   └── 1 node « master » élu : collecte + recalage spatial + reconstruction heatmap
│
└── Master → MQTT TLS → AOS-Core (192.168.0.176)
```

### Node ToF — Intérieur (ESP32-S3 + TMF8829)
| Élément | Spéc |
|---------|------|
| Capteur | AMS TMF8829 (dToF, mode 16×16 zones) |
| Portée | ~5 m intérieur (350 lux) |
| Couverture à 3 m hauteur | ~12-15 m², cellule ~8-10 cm au sol |
| MCU | ESP32-S3 |
| Firmware | ESP-IDF + FreeRTOS |
| Com | WiFi ESP-NOW (mesh) |
| Alim | Batterie (1×18650, ~3,5 Ah/an avec duty-cycle 60-120s) |
| Payload brut | `{zones: [[dist_cm]], confidence: %, ts, device_id}` |
| Fréquence TX | Sur delta significatif + heartbeat espaceé |
| RGPD | Aucune image — uniquement grille de distances |

### Node Radar — Extérieur (LD6001A)
| Élément | Spéc |
|---------|------|
| Capteur | LD6001A (60 GHz mmWave, time-to-market prioritaire) |
| Portée | 8-15 m, insensible lumière/pluie/nuit |
| Capacité comptage | ≤ 10 personnes (fallback IWR6843AOP si >10 requis) |
| MCU | Intégré au module radar |
| Com | WiFi ESP-NOW (mesh) |
| Alim | Panneau solaire + batterie (3 jours autonomie sans soleil) |
| Payload brut | `{targets: [{x, y, z, v}], count, ts, device_id}` |
| Fréquence TX | 1×/10s |
| RGPD | Aucune image — points 3D uniquement |

### Master — Reconstruction spatiale
- **1 node élu** dans le mesh (plus puissant ou mieux placé)
- Rôle : collecte les payloads bruts de tous les nodes → recalage spatial (pose de chaque node sur plan 2D villa) → fusion ToF + radar → heatmap unifiée → MQTT vers backend
- Évite de saturer le lien WiFi unique vers AOS-Core avec 20 flux individuels
- Si chute du master → réélection automatique dans le mesh

### Backend (AOS-Core, 192.168.0.176)
| Service | Rôle |
|---------|------|
| `ingest-mqtt` | Reçoit la heatmap déjà fusionnée du master → TimescaleDB |
| `alert-engine` | Seuils configurables : count > N → alerte Telegram/email/Dashboard |
| `pms-bridge` | Intégration PMS (Airbnb, Guesty, etc.) — corrélation réservation vs occupation |
| Dashboard | Extension SvelteKit : heatmap temps réel + overlay PMS + alertes |

## Roadmap
| Phase | Priorité | Contenu | Livrable |
|-------|----------|---------|----------|
| **P0** | 🔴 Immédiat | Scope client : extérieur, seuil vs comptage exact, exclusion ToF diurne → signé | Contrat/CDC signé |
| **P1** | 🟠 POC | Node ToF intérieur : ESP32-S3 + TMF8829, mesh ESP-NOW, master reconstruction | 1 node comptage fiable |
| **P2** | 🟠 POC | Node Radar extérieur : LD6001A, intégration mesh, reconstruction multi-nœuds | Comptage robuste plein soleil |
| **P3** | 🟡 Backend | ingest-mqtt + alert-engine + dashboard heatmap | Services Docker |
| **P4** | 🟡 PMS | Intégration Airbnb/Guesty — corrélation calendrier/occupation | PMS bridge fonctionnel |
| **P5** | 🟢 Pilote | Déploiement villa pilote, tests, itération | Client validé |
| **P6** | 🟢 Scale | SaaS multi-villas, BOM optimisé, industrialisation | 2e client |

## Réutilisation LVMH (partielle — adapter au TMF8829/LD6001A)
| Composant | Projet source | Statut |
|-----------|--------------|--------|
| ESP32-S3 + antenne | #062 MCU.kicad_sch | Base, adapter mesh |
| HDC1080 (T°C + humidité) | #062 SENSORS.kicad_sch | Réutilisé |
| Alim batterie | Nouveau | Circuit chargeur 18650 |
| MLX90640 (thermique) | #062 | **Non retenu** pour le SKU HeatMap |
| LD2460 (24 GHz) | #060 | Remplacé par LD6001A |

## Garde-fous
1. ❌ Ne jamais proposer du ToF pour extérieur diurne — physique impossible
2. ❌ Ne jamais dimensionner bruit-continu sur batterie — pas dans ce SKU
3. ❌ Pas de caméra RGB/optique — RGPD by design
4. ❌ Pas d'enregistrement audio — SPL seulement
5. ❌ Ne pas créer de nouveau serveur/VM — AOS-Core existant
6. ❌ Ne pas modifier les schémas LVMH originaux sans copie préalable
7. 📌 Toute action rattachée au North Star — signaler le « framework gratuit »

## Prochaines actions immédiates
- [ ] P0 : Scope client signé (seuil vs comptage exact, zones extérieures)
- [ ] Télécharger datasheets : TMF8829, LD6001A, BME680, ESP32-S3 → `docs/datasheets/`
- [ ] Recherche PMS : API Airbnb, Guesty, hubs → `docs/pms/`
- [ ] Prototype mesh ESP-NOW : 2 nodes + master election + reconstruction basique
- [ ] Dimensionnement solaire + batterie pour node extérieur (3j autonomie)
