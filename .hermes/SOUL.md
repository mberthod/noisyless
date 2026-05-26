# SOUL.md — Hermes Agent NOISYLESS
## Instructions système pour un agent reprenant le projet

> Ce fichier contient le minimum vital pour qu'un agent Hermes sur une autre machine (ex: 192.168.0.176) puisse reprendre le développement NOISYLESS.

## Identité projet

Tu travailles sur **NOISYLESS**, startup de capteurs environnementaux pour locations courte durée (Airbnb).
Client : NOISYLESS / ELEC-CORE. Projets : #089 (capteur env), #091 (HeatMap).

## Infrastructure

| Élément | Valeur |
|---------|--------|
| Serveur prod | Hetzner CAX11, **91.99.26.43**, SSH `noisyless@91.99.26.43` |
| Domaine | **noisyless.com** (site vitrine) + **platform.noisyless.com** (dashboard) |
| Docker | `/opt/noisyless/docker-compose.yml` — 7 services |
| DB | TimescaleDB (PostgreSQL 16), tables `measurements`, `orders`, `devices`... |
| GitHub | `git@github.com:mberthod/noisyless.git` (branche `main`) |
| Kdrive local | `~/Kdrive/01_CLIENTS/NOISYLESS/` |
| Kdrive firmware | `~/Kdrive/01_CLIENTS/NOISYLESS/01_PROJETS/202604_089_.../05_FIRMWARE/` |

## Design system

| Élément | Valeur |
|---------|--------|
| Direction | INSTRUMENT DE PRÉCISION (Dieter Rams / Braun) |
| Police titres | Space Mono |
| Police corps | IBM Plex Sans |
| Fond | #FFFFFF |
| Texte | #1A1A1A |
| Accent | #FF6B00 (ambre, ≤5% surface) |
| Anti-patterns | Pas de gradient, pas de dark theme, pas d'emojis, pas de blob |

## Business model

3 abonnements obligatoires : Starter 4.99€ (1-5 capteurs), Business 14.90€ (6-20), Pro 39.90€ (21-100).
Overage : 0.50€/capteur/mois au-delà de 100.
Capteur NOISYLESS Simple : 79€.

## Conventions Kdrive

- Nommage : `YYYYMM_NNN_Description_Courte/` — pas `Projet_`, pas `__`
- Sous-dossiers : `01_CDC/`, `02_ETUDE/`, `03_CAO/`, `04_FAB/`, `05_FW/`, `06_SW/`, `99_DOCS/`
- Fichiers admin (devis, factures) dans `_admin/`

## Règles absolues

1. **Aucun envoi client sans validation humaine (T3)**
2. Hardware = Mathieu. Les agents ne créent pas de tâches hardware
3. `rsync` pour transferts vers Hetzner, pas `scp` (timeout)
4. `docker compose build X && docker compose up -d X` après modif code, pas `restart`
5. Priorité backlog : **web > API > infra > firmware**
6. `.env` de prod : `/opt/noisyless/.env` — ne jamais commiter
7. Cloudflare : purger le cache après chaque déploiement HTML

## Rituels

- `EditorBot` : article blog 6h tous les 2 jours
- `SCRIBE_Daily` : documentation projets 7h
- `Suivi_Projets` : rapport quotidien 8h
- `Facturation_Auto` : scan jalons 16h → génère factures

## Pièges connus

- `docker compose restart` ne recharge pas `.env` → utiliser `up -d`
- paho-mqtt v2 nécessite `callback_api_version=MQTTv5`
- FastAPI : import manquant = crash silencieux (NameError → restart loop)
- Cloudflare cache HTML → purge après rsync
- mqtt-ingest : callback dans un thread → utiliser `asyncio.Queue`
- Stripe : `pk_live_` = publique (JS), `sk_live_` = secrète (backend)
- CORS : ajouter headers dans nginx pour `/api/` et `/shop/`

## Fichiers clés

- Skills : `./.hermes/skills/noisyless-heatmap/SKILL.md` et `noisyless-deployment/SKILL.md`
- Backlog : `./.hermes/backlog/todo.json` (28 tâches, 21 done, 6 blocked, 1 error)
- Knowledge : `./.hermes/knowledge/elec-core/` (clients + projets)
- Cahier de recette : `./00_INFOS_CLIENT/CAHIER_RECETTE_NOISYLESS.md`
