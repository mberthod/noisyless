# 202504_Projet_045_noisylessadaptateur

## Contexte
- **Client** : NOISYLESS
- **Entité** : MBHREP/ELEC-CORE
- **Démarrage** : 2025-04
- **Localisation Kdrive** : `~/Kdrive/01_ELECTRONIQUE/202504_045_Adaptateur_Capteur/`
- **Dernière activité** : 2025-04-12 09:53:24
- **Phase actuelle** : CAO finalisée — statut fabrication inconnu

## Description
Adaptateur / carte d'interface pour le produit NOISYLESS. Projet de connexion/câblage probablement destiné à faciliter le déploiement ou l'interfaçage du capteur principal.

## Stack technique

### CAO — KiCad (fichiers présents)
| Fichier | Description |
|---------|-------------|
| `noisylessadaptateur.kicad_pro` | Projet KiCad |
| `noisylessadaptateur.kicad_sch` | Schématique |
| `noisylessadaptateur.kicad_pcb` | PCB layout |
| `noisylessadaptateur.kicad_prl` | Librairie locale |

### Production — fichiers générés
| Fichier | Description |
|---------|-------------|
| `production/bom.csv` | Bill of Materials |
| `production/positions.csv` | Fichier Pick & Place |
| `production/netlist.ipc` | Netlist IPC |
| `production/designators.csv` | Designators |
| `production/noisylessadaptateur.zip` | Archive Gerber + fichiers de production |
| `production/backups/noisylessadaptateur_2025-04-11_09-07-26.zip` | Backup production |

### Backups KiCad
- `noisylessadaptateur-2025-04-11_090722.zip`
- `noisylessadaptateur-2025-04-11_110321.zip`
- `noisylessadaptateur-2025-04-11_213914.zip`
- `noisylessadaptateur-2025-04-12_095323.zip`

## Livrables prévus / livrés
- ✅ Schématique, PCB, BOM, positions, netlist, Gerbers — tous présents dans `/03_CAO/noisylessadaptateur/production/`
- ❓ Statut fabrication : inconnu — les Gerbers ont-ils été envoyés en prod ? PCB reçu/testé ?

## Risques / sujets ouverts
- **Question critique** : l'adaptateur a-t-il été fabriqué physiquement, ou seulement conçu en CAO ?
- Si déjà fabriqué : où sont les photos, résultats de test, intégration avec le capteur principal ?
- Relation produit principal vs adaptateur — deux PCB séparés qui s'empilent ? Ou alternative ?
- Projet dormant depuis avril 2025 — à dépoussiérer si utile pour la HeatMap

## Prochaines actions
- [ ] Déterminer si le PCB a été fabriqué (vérifier emails JLCPCB/PCBWay, boîtes physiques)
- [ ] Si non fabriqué : évaluer s'il faut lancer la prod ou merger dans une seule carte
- [ ] Relever la BOM et le schéma pour documenter les composants dans cette fiche
