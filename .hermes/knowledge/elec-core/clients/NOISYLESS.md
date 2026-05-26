# NOISYLESS

## Identité
- **Entité ELEC-CORE/HEXACORE/MBHREP** : Produit interne MBHREP (filiale MBHHOLD)
- **Statut** : Actif — MVP V1 en finalisation
- **Site web** : https://noisyless.com (hébergement OVH FTP)
- **Serveur** : VPS Hetzner CAX11 — 91.99.26.43
- **Secteur / activité** : IoT — Monitoring environnemental (bruit, qualité d'air, présence) pour hôtellerie et immobilier
- **Agent commercial** : ARTEMIS (profil Hermes MBHOLD)
- **North Star** : Contribue aux +200 000 € perso d'ici décembre 2026

## Contacts
| Nom | Email | Téléphone | Rôle |
|-----|-------|-----------|------|
| Contact produit | contact@noisyless.com | — | — |

## Projets actifs
| NNN | Nom | Phase | Dernière activité |
|-----|-----|-------|-------------------|
| 89 | NOISYLESS_PROJET_PRINCIPAL | MVP V1 en finalisation | 2026-04-27 |
| 45 | noisylessadaptateur | CAO finalisée, fabrication ? | 2025-04-12 |
| 91 | NOISYLESS HeatMap | Conception — étude préliminaire | 2026-05-22 |

## Pipeline commercial
| Client | Potentiel | Statut |
|--------|-----------|--------|
| Ibis Managed Services | 15 000 € | Actif |
| Adagio Access | 18 000 € | Actif |
| Starhotels | 15 000 € | Actif |
| Elior Residences | 12 000 € | Draft accepté ✅ |
| La Cite Urbaine | 10 000 € | Actif |
| GuestReady, Welkeys, Ouikey, Keylodge, HostnFly | À qualifier | Prospect |

## Prospection élargie
Fichiers de référence :
- `~/prospects_hotels_noisyless.md` — 20 groupes hôteliers français (B&B, Best Western, Odalys, PVCP, Marriott, Hilton...)
- `~/prospects_noisyless.md` — 20 prospects alternatifs (résidences étudiantes, seniors, tourisme, property managers)

## Pricing
| Offre | Prix |
|-------|------|
| Starter | 4,99 €/mois |
| HeatMap upsell | +10-20 €/mois/logement |
| Modèle | Abonnement mensuel récurrent, pas d'essai gratuit |

## Synthèse business
- Produit IoT avec stack complète : hardware ESP32, firmware PlatformIO, backend Hetzner (TimescaleDB + FastAPI), dashboard SvelteKit
- Positionnement « écosystème du confort » vs concurrent Minut (mono-fonction)
- Pipeline commercial actif : 5 clients qualifiés (~70 000 € potentiel)
- Extension HeatMap (projet #091) = différenciant premium pour villas/locations haut de gamme

## Sujets ouverts / risques
- 5 drafts emails en attente validation T3 (Elior Residences déjà accepté)
- Documenter où se trouvent les sources hardware du projet principal (Git? 01_ELECTRONIQUE?)
- Statut fabrication de l'adaptateur #045 à clarifier
- Alignement à confirmer : l'adaptateur #045 est-il un PCB séparé ou une version antérieure du capteur principal ?

## Arborescence Kdrive
| Chemin | Contenu |
|--------|---------|
| `01_CLIENTS/NOISYLESS/202604_089_Capteur_Environnemental_V1/` | Projet #089 — CAO, firmware, boîtier 3D, Git |
| `01_CLIENTS/NOISYLESS/202605_091_HeatMap_Presence/` | Projet #091 — specs, docs, firmware, backend |
| `01_CLIENTS/NOISYLESS/_admin/emails/` | Étude concurrence (.pptx) |
| `01_ELECTRONIQUE/202504_045_Adaptateur_Capteur/` | KiCad, BOM, Gerbers adaptateur |

## Sources externes (Hermes MBHOLD)
- `~/.hermes/knowledge/noisyless/` — 12 fichiers (projets, doctrine, décisions, clients, agents)
- `~/.hermes/skills/strategy/north-star/references/noisyless-*.md` — vérité produit, déploiement FTP, infra, market intel
