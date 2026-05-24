# NOISYLESS — Brief de déploiement (handoff agents)

> Paquet autonome destiné à un système d'agents (orchestrateur + exécutants).
> Tout le contexte nécessaire est ici : aucun besoin de l'historique de conversation.
> Backlog exécutable : voir `tasks.json`. Bibliothèque de prompts : `PROMPTS.md`.

---

## 0. North Star (filtre de décision)

Objectif : **+200 000 € de revenu personnel d'ici décembre 2026** (~7 mois restants au 24/05/2026).
Règle : toute tâche doit contribuer au cash avant décembre. Le hardware neuf nécessitant
certification = revenu 2027. Priorité absolue : **activer et vendre l'existant**.

Cette ligne (NOISYLESS) est **une part** du North Star, pas la totalité.

---

## 1. Produit & société

- **Marque** : NOISYLESS — capteurs IoT pour location courte durée (Airbnb) et conciergeries.
- **Promesse** : repérer fêtes, suroccupation et qualité d'air, **sans caméra ni micro**, 100 % RGPD.
- **Éditeur** : MBHREP (EURL), RCS Périgueux, capital 100 €, 24 rue du Général Lamy, Périgueux.
- **Contacts** : contact@noisyless.com · 06 17 57 68 81 · dashboard : platform.noisyless.com
- **Hébergement** : OVH (France) + TimescaleDB (Allemagne), aucun transfert hors UE, DPO désigné.
- **Sécurité** : MQTT over TLS 1.3, JWT 256-bit, NVS chiffré sur devices.

### Gamme (état au 24/05/2026)
| Produit | Prix | Statut | Capteurs |
|---|---|---|---|
| NOISYLESS Simple | 79 € | **En vente** | bruit dB SPL (IEC 61672-1 cl.2), présence PIR, CO₂/T°/humidité, WiFi+BT |
| HeatMap Intérieur | 129 € | Précommande | matrice ToF 16×16, comptage multi-personnes, mesh ESP-NOW |
| HeatMap Extérieur | 199 € | Précommande | radar 60 GHz (30 m), IP65, tracking 3D, PoE |
| Détecteur Fuite + Vanne | 149 € | Précommande | détection eau, vanne motorisée, coupure auto < 5 s |

### Abonnements
Sans abo · Starter 4,99 €/mois (5 capteurs) · Pro 29,90 €/mois (illimité, webhooks/API, Telegram/SMS).

---

## 2. État réel du site (audit, données Google Search Console)

- 🔴 **P0 — Checkout cassé** : la page affiche « Erreur lors du paiement ». **Aucune commande ne passe.** Bloqueur n°1.
- 🟠 **Visibilité quasi nulle** : 1 page indexée / 3 non indexées / **4 clics au total**.
- 🟠 **Site = one-pager à ancres** (`#produit`, `#shop`…) → une seule URL indexable, impossible de ranker sur plusieurs intentions.
- 🟠 **0 extrait produit** : aucune donnée structurée Product/Offer.
- 🟠 **Drapeau « Non-HTTPS »** dans GSC : vérifier redirection/canonical.
- 🟡 **Bugs de copie** : « images识别ables » (caractères corrompus → « identifiables ») ; « surpopulation » vs « suroccupation » (garder « suroccupation »).
- 🟡 **Mot-clé money absent du titre** : aligner sur « détecteur de bruit Airbnb ».
- 🟡 **Pas de FAQ ni de blog** : manque GEO majeur.
- ℹ️ **Confusion de marque** noisyless ↔ noiseless (les moteurs « corrigent » le nom).

Note : un fetcher tiers recevait un 403, mais GSC confirme que Googlebot indexe la page → l'accès crawler n'est pas le problème ; le problème est structurel/contenu/conversion.

---

## 3. Plan priorisé (détail exécutable dans `tasks.json`)

- **P0 — Encaisser** : réparer le paiement ; décider/activer les précommandes vendables.
- **P1 — Visibilité** : éclater le one-pager en vraies pages ; robots.txt ; sitemap + soumission ; HTTPS/canonical ; données structurées ; forçage d'indexation ; traiter les 3 pages non indexées.
- **P2 — Contenu & GEO** : corriger la copie ; publier les 9 articles + index blog ; article pilier ; page FAQ ; /llms.txt ; pages légales (via prompts + validation juridique) ; désambiguïsation de marque.

---

## 4. Contenu de ce paquet

- `tasks.json` — backlog structuré, dispatchable (id, priorité, livrable, critères d'acceptation, dépendances, agent suggéré).
- `PROMPTS.md` — bibliothèque : audit SEO/GEO, indexation ultime, moteur de blog, 3 pages légales.
- `blog/` — 9 articles Markdown prêts à publier (frontmatter + JSON-LD + maillage interne + CTA).
- `assets/llms.txt` — fichier à déployer sur `/llms.txt`.
- `assets/product-jsonld.json` — JSON-LD Product/Offer pour le capteur Simple (à dupliquer par produit).
- `roadmap.md` — feuille de route 2026 sur 3 phases.

---

## 5. Contraintes impératives (à respecter par tous les agents)

1. **Jamais de caméra ni de micro d'enregistrement** dans le discours produit (positionnement et conformité).
2. **Données en UE uniquement** ; mentionner OVH France + Allemagne.
3. **Pages légales = validation juridique humaine obligatoire** avant mise en ligne (ne pas publier les sorties brutes des prompts).
4. **Comparatifs concurrents** : vérifier specs/prix sur les sites officiels avant publication (ils changent).
5. **Orthographe exacte** « NOISYLESS » partout (anti-confusion noiseless).
6. **Ne rien publier qui ne contribue pas au North Star** ; en cas de doute, escalader.
